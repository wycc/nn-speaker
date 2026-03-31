# Exercise 2 開發任務清單

## 1. 目標

將 `ESP32 記憶體觀察與配置` 規格轉成可執行的工程任務，供實作者逐項完成、驗證與交付。

## 2. 任務拆解

### A. 環境確認

- [ ] 確認目標板卡為支援 PSRAM 的 ESP32 裝置。
- [ ] 確認 `platformio.ini` 已啟用 `BOARD_HAS_PSRAM`。
- [ ] 確認專案可正常輸出 Serial log。
- [ ] 確認可引用 `esp_heap_caps.h`。

完成定義：

- 可在目前 `firmware` 專案中編譯 Arduino/ESP32 程式。
- 可存取 Internal、DMA、PSRAM 相關 API。

### B. 記憶體觀測模組骨架

- [ ] 新增獨立模組檔案，例如 `src/exercises/MemoryObservationExercise.h`。
- [ ] 新增對應實作檔 `src/exercises/MemoryObservationExercise.cpp`。
- [ ] 定義資料結構保存 before/after snapshot。
- [ ] 定義資料結構保存三種配置指標與大小。
- [ ] 提供單一入口，例如 `runOnce()`。

完成定義：

- 模組可被 `setup()` 呼叫。
- 模組本身不依賴目前語音主流程即可運作。

### C. 記憶體查詢功能

- [ ] 實作 `captureSnapshot()`。
- [ ] 查詢 Heap total size。
- [ ] 查詢 Heap free size。
- [ ] 查詢 Internal free size。
- [ ] 查詢 PSRAM free size。
- [ ] 查詢 `MALLOC_CAP_INTERNAL` free size。
- [ ] 查詢 `MALLOC_CAP_8BIT` free size。
- [ ] 查詢 `MALLOC_CAP_DMA` free size。
- [ ] 查詢 `MALLOC_CAP_SPIRAM` free size。
- [ ] 查詢各 capability 的 largest free block。

完成定義：

- 能在單一 snapshot 中取得完整觀測欄位。
- 欄位名稱與輸出順序固定。

### D. 記憶體配置功能

- [ ] 配置 `16 KB` Internal SRAM。
- [ ] 配置 `8 KB` DMA memory。
- [ ] 配置 `64 KB` PSRAM。
- [ ] 針對每次配置加入成功/失敗檢查。
- [ ] 配置成功後保留指標，供後續釋放。

完成定義：

- 三筆配置邏輯皆使用 `heap_caps_malloc()`。
- 任一配置失敗時，輸出可辨識錯誤訊息。

### E. 輸出與比較

- [ ] 印出 `=== Before Allocation ===` 區段。
- [ ] 印出 before snapshot。
- [ ] 執行配置。
- [ ] 印出 `=== After Allocation ===` 區段。
- [ ] 印出 after snapshot。
- [ ] 印出 delta 或至少可人工比較的欄位。
- [ ] 額外輸出每個 buffer 的配置結果。

完成定義：

- Serial Monitor 可明確看出配置前後差異。
- 教師可直接截圖用於評分。

### F. 資源釋放與健全性

- [ ] 釋放 Internal buffer。
- [ ] 釋放 DMA buffer。
- [ ] 釋放 PSRAM buffer。
- [ ] 避免重複釋放。
- [ ] 保留空指標檢查。

完成定義：

- 單次測試結束後不留下不必要資源佔用。
- 模組可重複呼叫而不出現明顯未定義行為。

### G. 專案整合

- [ ] 決定整合方式：臨時從 `setup()` 呼叫，或用編譯旗標切換。
- [ ] 確保不破壞原本語音專案主流程。
- [ ] 若採條件切換，加入清楚註解與啟用方式。

完成定義：

- Exercise 2 可以單獨執行。
- 不需要永久改寫既有產品主流程。

### H. 驗證與交付

- [ ] 編譯成功。
- [ ] 實機執行成功。
- [ ] 取得 Serial output 截圖。
- [ ] 撰寫 1 至 2 頁報告。
- [ ] 檢查輸出是否足以支撐報告分析。

完成定義：

- 已具備程式碼、截圖、報告三項交付物。

## 3. 建議實作順序

1. 先完成模組骨架與 snapshot 結構。
2. 再完成 before/after 查詢輸出。
3. 接著加入三種 capability 配置。
4. 最後補上錯誤處理、釋放與整合方式。

## 4. 驗收檢查表

- [ ] `printMemoryStatus()` 或等價函式存在。
- [ ] `heap_caps_malloc()` 用於 Internal、DMA、PSRAM 三種配置。
- [ ] `Before Allocation` 與 `After Allocation` 皆有輸出。
- [ ] Heap / Internal / PSRAM 數值可比較。
- [ ] largest free block 有輸出或已明確說明未納入原因。
- [ ] 配置失敗時有錯誤訊息。
- [ ] 已釋放成功配置的記憶體。
- [ ] 截圖內容足以支撐評分與報告。

## 5. 對目前 firmware 專案的建議整合方式

建議採最小侵入做法：

1. 先把 Exercise 2 實作成獨立模組。
2. 需要驗證時，再於 `setup()` 中暫時呼叫。
3. 完成作業後，可移除該呼叫或改以編譯旗標控制。

這樣可避免直接干擾目前 `DIY Alexa` 主流程。
