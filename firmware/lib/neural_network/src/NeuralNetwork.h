#ifndef __NeuralNetwork__
#define __NeuralNetwork__

#include <stdint.h>

namespace tflite
{
    template <unsigned int tOpCount>
    class MicroMutableOpResolver;
    class ErrorReporter;
    class Model;
    class MicroInterpreter;
} // namespace tflite

struct TfLiteTensor;

class NeuralNetwork
{
private:
    tflite::MicroMutableOpResolver<10> *m_resolver;
    tflite::ErrorReporter *m_error_reporter;
    const tflite::Model *m_model;
    tflite::MicroInterpreter *m_interpreter;
    TfLiteTensor *input;
    TfLiteTensor *output;
    uint8_t *m_tensor_arena;

public:
    NeuralNetwork();                                       // uses built-in converted_model_tflite (wake word)
    // for hw3 modified: accepts any model data so DetectRecordWordState can
    // load model_recordword without a separate NeuralNetwork subclass
    NeuralNetwork(const unsigned char *model_data);
    ~NeuralNetwork();
    float *getInputBuffer();
    float predict();
};

#endif