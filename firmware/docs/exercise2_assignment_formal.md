# Exercise 2 作業規格書正式版

## 一、作業名稱

Exercise 2：ESP32 記憶體觀察與配置

## 二、作業目的

本作業旨在要求學生透過實作與觀測，理解 ESP32 在不同記憶體能力標記（capability）下的查詢與配置方式，並建立基礎的系統資源監控觀念。學生完成本作業後，應具備下列能力：

1. 理解 ESP32 內部 SRAM、PSRAM、DMA 記憶體之差異。
2. 熟悉 ESP-IDF / Arduino on ESP32 提供的記憶體查詢 API。
3. 正確使用 `heap_caps_malloc()` 進行 capability-based allocation。
4. 能比較記憶體配置前後的變化。
5. 能以輸出結果支撐基本分析與報告撰寫。

## 三、作業背景

ESP32 具備多種不同性質的記憶體區域，不同區域適用於不同應用情境，例如一般運算、DMA 傳輸、或外部大容量記憶體擴充。若開發者無法辨識各類記憶體特性，容易在系統設計時發生容量估算錯誤、記憶體不足、或效能不穩定等問題。

本作業聚焦於兩件事：

- 正確讀取系統目前的記憶體狀態。
- 正確地將不同型態的資料配置到對應的記憶體區。

## 四、作業要求

學生須撰寫一支 ESP32 程式，完成以下三部分內容。

### Part 1：顯示記憶體資訊

請實作一個函式，例如 `printMemoryStatus()`，並於 Serial Monitor 輸出當前系統記憶體資訊。輸出內容至少須包含：

- Heap total size
- Heap free size
- Internal SRAM 剩餘量
- PSRAM 剩餘量
- `MALLOC_CAP_INTERNAL`
- `MALLOC_CAP_8BIT`
- `MALLOC_CAP_DMA`
- `MALLOC_CAP_SPIRAM`

建議使用但不限於下列 API：

- `heap_caps_get_free_size(...)`
- `heap_caps_get_largest_free_block(...)`
- `ESP.getHeapSize()`
- `ESP.getFreeHeap()`
- `ESP.getPsramSize()`
- `ESP.getFreePsram()`

### Part 2：配置不同類型記憶體

請使用 `heap_caps_malloc(size, capability)` 進行記憶體配置，至少完成下列三項：

| 記憶體類型 | Capability | 建議大小 |
| ----- | ----- | ----- |
| Internal SRAM | `MALLOC_CAP_INTERNAL` | 16 KB |
| DMA memory | `MALLOC_CAP_DMA` | 8 KB |
| PSRAM | `MALLOC_CAP_SPIRAM` | 64 KB |

實作時須檢查每次配置結果是否成功，若失敗，需輸出清楚錯誤訊息。

### Part 3：觀察記憶體變化

程式必須依下列流程執行：

1. 程式啟動後輸出一次記憶體狀態。
2. 配置多種類型記憶體。
3. 再次輸出記憶體狀態。
4. 比較配置前後差異。

## 五、輸出格式要求

學生須於 Serial Monitor 中輸出明確可讀的結果。輸出格式至少需能對應如下結構：

```text
=== Before Allocation ===
Heap free: XXXXX
Internal free: XXXXX
PSRAM free: XXXXX
...

=== After Allocation ===
Heap free: XXXXX
Internal free: XXXXX
PSRAM free: XXXXX
...
```

建議補充：

- 各 capability 的 free size
- 各 capability 的 largest free block
- 各配置區塊的成功與失敗訊息

## 六、實作規範

學生提交之程式應符合下列要求：

1. 程式結構清楚，查詢、配置、輸出邏輯分離。
2. 需使用 capability-based allocation，不得以一般 `malloc()` 取代核心要求。
3. 配置結果需有錯誤處理，不可忽略 `nullptr`。
4. 變數命名與輸出欄位應具可讀性。
5. 建議於觀察完成後釋放已配置資源，以保留程式完整性。

## 七、繳交內容

學生須提交以下三項：

1. 完整程式碼檔案（`.cpp` 或 `.ino`）
2. 執行結果截圖（Serial output）
3. 1 至 2 頁簡短報告

## 八、報告要求

報告至少須包含以下內容：

1. 各種記憶體差異說明
2. 配置前後變化分析
3. 觀察到的現象，例如 fragmentation、largest free block 變化，或不同 capability 的配置特性

## 九、評分標準

| 評分項目 | 比例 |
| ----- | ----- |
| 記憶體資訊顯示正確 | 30% |
| 正確使用 `heap_caps_malloc` | 25% |
| 記憶體變化觀察 | 20% |
| 程式結構與可讀性 | 15% |
| 報告分析 | 10% |

## 十、提交方式

學生應依課程要求完成以下流程：

1. 從指定 repository fork 專案。
2. 完成 Part 1 至 Part 3 內容。
3. 建立 pull request 回原始 repository。
4. 於作業系統中提交 fork repository URL。

## 十一、教師驗收重點

教師審查時，建議優先確認以下事項：

1. 是否完整顯示所要求的記憶體資訊。
2. 是否正確使用 `heap_caps_malloc()` 配置三種記憶體。
3. 是否能從輸出中清楚看出配置前後差異。
4. 是否具備基本錯誤處理與清楚程式結構。
5. 報告是否能合理解釋輸出結果，而非僅貼上截圖。

## 十二、備註

若開發板不支援或未啟用 PSRAM，學生應於程式與報告中明確說明限制；但若本作業指定板卡已支援 PSRAM，則不應略過 PSRAM 部分。
