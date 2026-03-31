# Exercise 2 報告：ESP32 記憶體觀察與配置

## 1. 作業目的

本作業使用 ESP-WROVER-KIT 與 Arduino on ESP32，觀察 ESP32 在不同記憶體 capability 下的可用容量變化，並使用 `heap_caps_malloc()` 分別配置 Internal SRAM、DMA memory 與 PSRAM，最後比較配置前後的系統記憶體狀態。

本次作業完成內容如下：

- 顯示 Heap、Internal SRAM、PSRAM 與 capability 對應的 free size。
- 使用 `heap_caps_malloc()` 配置三種不同類型記憶體。
- 比較 `Before Allocation` 與 `After Allocation` 的數值差異。
- 釋放已配置記憶體，確認程式可以穩定重複執行。

## 2. 實作環境

- Board: ESP-WROVER-KIT
- Framework: Arduino on ESP32
- PlatformIO environment: `esp-wrover-kit`
- Serial Monitor: `115200 8-N-1`

## 3. 記憶體配置內容

本次使用下列 capability 進行記憶體配置：

| 記憶體類型 | Capability | 配置大小 |
| --- | --- | ---: |
| Internal SRAM | `MALLOC_CAP_INTERNAL` | 16 KB |
| DMA memory | `MALLOC_CAP_DMA` | 8 KB |
| PSRAM | `MALLOC_CAP_SPIRAM` | 64 KB |

實際執行結果如下：

- Internal SRAM allocation success: `16384 bytes`
- DMA allocation success: `8192 bytes`
- PSRAM allocation success: `65536 bytes`

## 4. 執行截圖與輸出

本次作業的 Serial output 顯示程式可重複執行，並能穩定輸出配置前後資訊。以下節錄實際執行結果：

```text
=== Exercise 2: ESP32 Memory Observation ===
=== Before Allocation ===
Heap total:                  371140 bytes
Heap free:                   345532 bytes
Internal free:               345532 bytes
PSRAM free:                  4192123 bytes
MALLOC_CAP_INTERNAL:         345532 bytes
MALLOC_CAP_8BIT:             4470187 bytes
MALLOC_CAP_DMA:              278064 bytes
MALLOC_CAP_SPIRAM:           4192123 bytes
Largest internal block:      114676 bytes
Largest 8-bit block:         4128756 bytes
Largest DMA block:           114676 bytes
Largest SPIRAM block:        4128756 bytes

=== Allocation Result ===
Internal SRAM allocation success: 16384 bytes at 0x4008F874
DMA allocation success: 8192 bytes at 0x3FFB2330
PSRAM allocation success: 65536 bytes at 0x3F800884

=== After Allocation ===
Heap total:                  371108 bytes
Heap free:                   320924 bytes
Internal free:               320924 bytes
PSRAM free:                  4126571 bytes
MALLOC_CAP_INTERNAL:         320924 bytes
MALLOC_CAP_8BIT:             4396427 bytes
MALLOC_CAP_DMA:              269856 bytes
MALLOC_CAP_SPIRAM:           4126571 bytes
Largest internal block:      114676 bytes
Largest 8-bit block:         4063220 bytes
Largest DMA block:           114676 bytes
Largest SPIRAM block:        4063220 bytes

=== Allocation Delta ===
Heap total:                  before=371140 after=371108 delta=-32
Heap free:                   before=345532 after=320924 delta=-24608
Internal free:               before=345532 after=320924 delta=-24608
PSRAM free:                  before=4192123 after=4126571 delta=-65552
MALLOC_CAP_INTERNAL:         before=345532 after=320924 delta=-24608
MALLOC_CAP_8BIT:             before=4470187 after=4396427 delta=-73760
MALLOC_CAP_DMA:              before=278064 after=269856 delta=-8208
MALLOC_CAP_SPIRAM:           before=4192123 after=4126571 delta=-65552
Largest internal block:      before=114676 after=114676 delta=0
Largest 8-bit block:         before=4128756 after=4063220 delta=-65536
Largest DMA block:           before=114676 after=114676 delta=0
Largest SPIRAM block:        before=4128756 after=4063220 delta=-65536

=== Release Result ===
All allocated buffers have been released.
```

## 5. 各種記憶體差異說明

### 5.1 Internal SRAM

Internal SRAM 是 ESP32 內部記憶體，速度快，適合一般運算與系統執行。這次使用 `MALLOC_CAP_INTERNAL` 配置 16 KB，配置後 `Internal free` 從 `345532 bytes` 降到 `320924 bytes`，顯示 internal 類記憶體確實被使用。

### 5.2 DMA memory

DMA memory 是可供 DMA 使用的記憶體區域，通常來自內部可存取記憶體。本次配置 8 KB 後，`MALLOC_CAP_DMA` 從 `278064 bytes` 降到 `269856 bytes`，差值為 `-8208 bytes`，與 8 KB 配置量相符，表示 DMA capability 記憶體成功配置。

### 5.3 PSRAM

PSRAM 是外部擴充記憶體，容量較大，適合存放較大資料塊，但速度通常低於內部 SRAM。本次配置 64 KB 後，`PSRAM free` 與 `MALLOC_CAP_SPIRAM` 都由 `4192123 bytes` 降到 `4126571 bytes`，差值為 `-65552 bytes`，可確認 PSRAM 正常啟用並可供程式配置。

## 6. 分配後變化分析

### 6.1 Heap 與 Internal 記憶體變化

`Heap free` 與 `Internal free` 都下降 `24608 bytes`。這個數值大於單純 16 KB Internal SRAM，原因是本次同時配置了 8 KB DMA memory，而 DMA 記憶體本身也會從內部可用記憶體區域取得，因此兩者一起造成 internal 類型記憶體下降。

### 6.2 PSRAM 變化

`PSRAM free` 下降 `65552 bytes`，接近 64 KB，代表外部記憶體配置成功。這也證明板子上的 PSRAM 設定與 capability 分配皆正常運作。

### 6.3 MALLOC_CAP_8BIT 變化

`MALLOC_CAP_8BIT` 從 `4470187 bytes` 降到 `4396427 bytes`，差值 `-73760 bytes`。由於 8-bit accessible 記憶體可涵蓋多種可位元組存取區域，因此其變化同時反映了 internal 與 PSRAM 配置所造成的總體消耗。

## 7. 觀察到的現象

### 7.1 Fragmentation 現象

這次輸出中：

- `Largest internal block` 差值為 `0`
- `Largest DMA block` 差值為 `0`
- `Largest SPIRAM block` 差值為 `-65536`

從結果來看，Internal 與 DMA 區域的最大連續區塊沒有明顯下降，表示這次配置沒有在內部記憶體造成明顯 fragmentation。反而在 PSRAM 區域中，最大連續區塊下降約 64 KB，這與配置單一大塊 PSRAM 記憶體相符，屬於預期結果，而不是異常碎片化。

### 7.2 Heap total 微小變化

`Heap total` 從 `371140` 變成 `371108`，差值 `-32 bytes`。這通常是配置過程中 allocator metadata 或系統管理開銷造成的小幅變化，屬正常現象。

### 7.3 可重複執行

Serial output 中同一組結果會週期性再次出現，且每次都能正確完成配置、比較與釋放，表示本程式已具備穩定重複執行能力。

## 8. 結論

本次作業成功完成 Part1 到 Part3 的要求：

- 已正確顯示各類記憶體資訊
- 已使用 `heap_caps_malloc()` 配置 Internal SRAM、DMA memory 與 PSRAM
- 已比較配置前後的變化
- 已觀察 largest free block 的變化並分析 fragmentation
- 已釋放配置後記憶體，確保程式可重複執行

整體結果證明 ESP32 可以透過 capability-based allocation 精確控制不同類型的記憶體配置，並可透過 Serial output 清楚觀察配置行為與系統資源變化。
