import tensorflow as tf
import numpy as np
from tensorflow.io import gfile
import tensorflow_io as tfio
from tensorflow.python.ops import gen_audio_ops as audio_ops
from tqdm import tqdm
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt

SPEECH_DATA='speech_data'
EXPECTED_SAMPLES=16000
NOISE_FLOOR=0.1
MINIMUM_VOICE_LENGTH=EXPECTED_SAMPLES/4

words = [
    'backward', 'bed', 'bird', 'cat', 'dog', 'down', 'eight', 'five',
    'follow', 'forward', 'four', 'go', 'happy', 'house', 'learn', 'left',
    'marvin', 'nine', 'no', 'off', 'on', 'one', 'right', 'seven', 'sheila',
    'six', 'stop', 'three', 'tree', 'two', 'up', 'visual', 'wow', 'yes',
    'zero', '_background',
]

def get_files(word):
    return gfile.glob(SPEECH_DATA + '/'+word+'/*.wav')

def get_voice_position(audio, noise_floor):
    audio = audio - np.mean(audio)
    audio = audio / np.max(np.abs(audio))
    return tfio.audio.trim(audio, axis=0, epsilon=noise_floor)

def get_voice_length(audio, noise_floor):
    position = get_voice_position(audio, noise_floor)
    return (position[1] - position[0]).numpy()

def is_voice_present(audio, noise_floor, required_length):
    return get_voice_length(audio, noise_floor) >= required_length

def is_correct_length(audio, expected_length):
    return (audio.shape[0]==expected_length).numpy()

def is_valid_file(file_name):
    audio_tensor = tfio.audio.AudioIOTensor(file_name)
    if not is_correct_length(audio_tensor, EXPECTED_SAMPLES):
        return False
    audio = tf.cast(audio_tensor[:], tf.float32)
    audio = audio - np.mean(audio)
    audio = audio / np.max(np.abs(audio))
    if not is_voice_present(audio, NOISE_FLOOR, MINIMUM_VOICE_LENGTH):
        return False
    return True

def get_spectrogram(audio):
    audio = audio - np.mean(audio)
    audio = audio / np.max(np.abs(audio))
    spectrogram = audio_ops.audio_spectrogram(audio, window_size=320, stride=160, magnitude_squared=True).numpy()
    spectrogram = tf.nn.pool(
        input=tf.expand_dims(spectrogram, -1),
        window_shape=[1, 6], strides=[1, 6],
        pooling_type='AVG', padding='SAME')
    spectrogram = tf.squeeze(spectrogram, axis=0)
    spectrogram = np.log10(spectrogram + 1e-6)
    return spectrogram

def process_file(file_path):
    audio_tensor = tfio.audio.AudioIOTensor(file_path)
    audio = tf.cast(audio_tensor[:], tf.float32)
    audio = audio - np.mean(audio)
    audio = audio / np.max(np.abs(audio))
    voice_start, voice_end = get_voice_position(audio, NOISE_FLOOR)
    end_gap=len(audio) - voice_end
    random_offset = np.random.uniform(0, voice_start+end_gap)
    audio = np.roll(audio,-random_offset+end_gap)
    background_volume = np.random.uniform(0, 0.1)
    background_files = get_files('_background_noise_')
    background_file = np.random.choice(background_files)
    background_tensor = tfio.audio.AudioIOTensor(background_file)
    background_start = np.random.randint(0, len(background_tensor) - 16000)
    background = tf.cast(background_tensor[background_start:background_start+16000], tf.float32)
    background = background - np.mean(background)
    background = background / np.max(np.abs(background))
    audio = audio + background_volume * background
    return get_spectrogram(audio)

train = []
validate = []
test = []

TRAIN_SIZE=0.8
VALIDATION_SIZE=0.1
TEST_SIZE=0.1

def process_files(file_names, label, repeat=1):
    file_names = tf.repeat(file_names, repeat).numpy()
    return [(process_file(file_name), label) for file_name in tqdm(file_names, desc=f"label={label}", leave=False)]

def process_word(word, repeat=1):
    label = words.index(word)
    file_names = [file_name for file_name in tqdm(get_files(word), desc="Checking", leave=False) if is_valid_file(file_name)]
    np.random.shuffle(file_names)
    train_size=int(TRAIN_SIZE*len(file_names))
    validation_size=int(VALIDATION_SIZE*len(file_names))
    train.extend(process_files(file_names[:train_size], label, repeat=repeat))
    validate.extend(process_files(file_names[train_size:train_size+validation_size], label, repeat=repeat))
    test.extend(process_files(file_names[train_size+validation_size:], label, repeat=repeat))

for word in tqdm(words, desc="Processing words"):
    if '_' not in word:
        repeat = 15 if word == 'wow' else 1
        process_word(word, repeat=repeat)

print(len(train), len(test), len(validate))

def process_background(file_name, label):
    audio_tensor = tfio.audio.AudioIOTensor(file_name)
    audio = tf.cast(audio_tensor[:], tf.float32)
    audio_length = len(audio)
    samples = []
    for section_start in tqdm(range(0, audio_length-EXPECTED_SAMPLES, 8000), desc=file_name, leave=False):
        section_end = section_start + EXPECTED_SAMPLES
        section = audio[section_start:section_end]
        spectrogram = get_spectrogram(section)
        samples.append((spectrogram, label))
    for section_index in tqdm(range(1000), desc="Simulated Words", leave=False):
        section_start = np.random.randint(0, audio_length - EXPECTED_SAMPLES)
        section_end = section_start + EXPECTED_SAMPLES
        section = np.reshape(audio[section_start:section_end], (EXPECTED_SAMPLES))
        result = np.zeros((EXPECTED_SAMPLES))
        voice_length = np.random.randint(int(MINIMUM_VOICE_LENGTH/2), EXPECTED_SAMPLES)
        voice_start = np.random.randint(0, EXPECTED_SAMPLES - voice_length)
        hamming = np.hamming(voice_length)
        result[voice_start:voice_start+voice_length] = hamming * section[voice_start:voice_start+voice_length]
        spectrogram = get_spectrogram(np.reshape(section, (16000, 1)))
        samples.append((spectrogram, label))
    np.random.shuffle(samples)
    train_size=int(TRAIN_SIZE*len(samples))
    validation_size=int(VALIDATION_SIZE*len(samples))
    train.extend(samples[:train_size])
    validate.extend(samples[train_size:train_size+validation_size])
    test.extend(samples[train_size+validation_size:])

for file_name in tqdm(get_files('_background_noise_'), desc="Processing Background Noise"):
    process_background(file_name, words.index("_background"))

print(len(train), len(test), len(validate))

def process_problem_noise(file_name, label):
    samples = []
    audio_tensor = tfio.audio.AudioIOTensor(file_name)
    audio = tf.cast(audio_tensor[:], tf.float32)
    audio_length = len(audio)
    for section_start in tqdm(range(0, audio_length-EXPECTED_SAMPLES, 400), desc=file_name, leave=False):
        section_end = section_start + EXPECTED_SAMPLES
        section = audio[section_start:section_end]
        spectrogram = get_spectrogram(section)
        samples.append((spectrogram, label))
    np.random.shuffle(samples)
    train_size=int(TRAIN_SIZE*len(samples))
    validation_size=int(VALIDATION_SIZE*len(samples))
    train.extend(samples[:train_size])
    validate.extend(samples[train_size:train_size+validation_size])
    test.extend(samples[train_size+validation_size:])

for file_name in tqdm(get_files("_problem_noise_"), desc="Processing problem noise"):
    process_problem_noise(file_name, words.index("_background"))

def process_mar_sounds(file_name, label):
    samples = []
    audio_tensor = tfio.audio.AudioIOTensor(file_name)
    audio = tf.cast(audio_tensor[:], tf.float32)
    audio_length = len(audio)
    for section_start in tqdm(range(0, audio_length-EXPECTED_SAMPLES, 4000), desc=file_name, leave=False):
        section_end = section_start + EXPECTED_SAMPLES
        section = audio[section_start:section_end]
        section = section - np.mean(section)
        section = section / np.max(np.abs(section))
        background_volume = np.random.uniform(0, 0.1)
        background_files = get_files('_background_noise_')
        background_file = np.random.choice(background_files)
        background_tensor = tfio.audio.AudioIOTensor(background_file)
        background_start = np.random.randint(0, len(background_tensor) - 16000)
        background = tf.cast(background_tensor[background_start:background_start+16000], tf.float32)
        background = background - np.mean(background)
        background = background / np.max(np.abs(background))
        section = section + background_volume * background
        spectrogram = get_spectrogram(section)
        samples.append((spectrogram, label))
    np.random.shuffle(samples)
    train_size=int(TRAIN_SIZE*len(samples))
    validation_size=int(VALIDATION_SIZE*len(samples))
    train.extend(samples[:train_size])
    validate.extend(samples[train_size:train_size+validation_size])
    test.extend(samples[train_size+validation_size:])

for file_name in tqdm(get_files("_mar_sounds_"), desc="Processing mar sounds"):
    process_mar_sounds(file_name, words.index("_background"))

print(len(train), len(test), len(validate))

np.random.shuffle(train)
X_train, Y_train = zip(*train)
X_validate, Y_validate = zip(*validate)
X_test, Y_test = zip(*test)

np.savez_compressed("training_spectrogram.npz", X=X_train, Y=Y_train)
print("Saved training data")
np.savez_compressed("validation_spectrogram.npz", X=X_validate, Y=Y_validate)
print("Saved validation data")
np.savez_compressed("test_spectrogram.npz", X=X_test, Y=Y_test)
print("Saved test data")
print("Done!")
