# ESP32 記憶體觀察與配置作業規格書

## 1. 文件資訊

- 文件名稱：ESP32 記憶體觀察與配置作業規格書
- 文件版本：v1.0
- 文件性質：依據 `ai_application_2026_exec2.md` 整理之正式實作規格
- 適用對象：ESP32 韌體開發者、作業實作者、審查者
- 來源依據：`C:\Users\super\Downloads\ai_application_2026_exec2.md`

## 2. 目的

本規格書定義一個 ESP32 韌體程式之功能、輸出、驗收與交付要求。該程式需展示 ESP32 不同記憶體區域的查詢與配置行為，並在記憶體配置前後輸出可比較的觀測結果，以支援學習與驗證：

- ESP32 記憶體架構理解
- ESP-IDF / Arduino on ESP32 記憶體 API 使用
- `heap_caps_malloc` 的 capability-based allocation
- 記憶體使用變化觀察
- 基本系統資源監控能力

## 3. 範圍

本規格僅涵蓋以下內容：

- 查詢並顯示系統記憶體資訊
- 以指定 capability 分配三種記憶體
- 比較配置前後的記憶體變化
- 於 Serial Monitor 顯示結果
- 提供作業交付所需的程式、輸出與報告內容

本規格不涵蓋：

- 雲端服務整合
- 網路功能驗證
- 長時間壓力測試
- 複雜 GUI 或非序列埠輸出介面

## 4. 執行環境與前提

### 4.1 硬體環境

- 目標平台應為 ESP32 系列開發板
- 若需觀察 PSRAM，硬體須支援 PSRAM，且建置設定須啟用 PSRAM

### 4.2 軟體環境

- 開發框架：Arduino on ESP32 或 ESP-IDF 相容環境
- 可使用之核心 API 應包含：
  - `heap_get_free_size`
  - `heap_caps_get_free_size`
  - `heap_caps_get_largest_free_block`
  - `heap_caps_malloc`
  - `free`

### 4.3 本專案已知上下文

依目前專案設定，`platformio.ini` 顯示：

- 開發板：`esp-wrover-kit`
- Framework：`arduino`
- 已啟用：`BOARD_HAS_PSRAM`

因此，本規格可直接以「具 PSRAM 的 ESP32 專案」作為預設目標環境。

## 5. 名詞定義

- Heap：可供動態配置使用的記憶體池
- Internal SRAM：ESP32 內部 SRAM
- PSRAM：外部 SPI RAM
- DMA memory：可供 DMA 使用的記憶體區
- Capability：ESP32 heap allocator 用以篩選記憶體屬性的旗標
- Fragmentation：雖有剩餘記憶體，但大塊連續空間不足的現象

## 6. 系統行為總覽

系統應完成以下主流程：

1. 啟動系統與 Serial 通訊
2. 輸出第一次記憶體狀態
3. 依指定 capability 分配三塊記憶體
4. 驗證各配置是否成功
5. 輸出第二次記憶體狀態
6. 呈現配置前後差異
7. 保留必要資訊供截圖與報告分析

## 7. 功能需求

### 7.1 Part 1：記憶體資訊顯示

#### FR-001 記憶體狀態輸出函式

系統必須實作一個專責函式，例如 `printMemoryStatus()`，用於集中輸出當前記憶體狀態。

驗收標準：

- 呼叫一次函式即可輸出完整狀態
- 函式輸出內容具固定順序或固定標題
- 相同函式可於配置前與配置後重複呼叫

#### FR-002 Heap 基本資訊

系統必須輸出下列 Heap 資訊：

- Heap total size
- Heap free size

驗收標準：

- 兩項資訊均需出現在 Serial Monitor
- 數值需可辨識為位元組數或等價記憶體容量數值

#### FR-003 特定記憶體剩餘量

系統必須輸出下列剩餘量：

- Internal memory（SRAM）剩餘量
- PSRAM 剩餘量

驗收標準：

- Internal 與 PSRAM 須各自獨立列示
- 若平台支援 PSRAM，PSRAM 數值不可省略

#### FR-004 Capability 類別資訊

系統必須輸出以下 capability 對應之可用記憶體資訊：

- `MALLOC_CAP_INTERNAL`
- `MALLOC_CAP_8BIT`
- `MALLOC_CAP_DMA`
- `MALLOC_CAP_SPIRAM`

驗收標準：

- 四類 capability 均需在輸出中可識別
- 每項 capability 至少應輸出 free size
- 建議同時輸出 largest free block，以利觀察 fragmentation

#### FR-005 查詢 API 使用

系統應優先使用以下 API 完成資訊查詢：

- `heap_get_free_size()`
- `heap_caps_get_free_size(MALLOC_CAP_INTERNAL)`
- `heap_caps_get_free_size(MALLOC_CAP_SPIRAM)`
- `heap_caps_get_largest_free_block(...)`

說明：

- 本需求為來源文件之建議 API，實作時可視框架包裝差異調整，但輸出資訊不得減少

### 7.2 Part 2：不同類型記憶體配置

#### FR-006 配置 API

系統必須使用 `heap_caps_malloc(size, capability)` 進行記憶體配置。

驗收標準：

- 不得僅以一般 `malloc` 取代全部配置需求
- 三種指定記憶體皆須以 capability 方式明確配置

#### FR-007 配置目標一：Internal SRAM

系統必須配置一塊 Internal SRAM 記憶體。

- 指定 capability：`MALLOC_CAP_INTERNAL`
- 建議配置大小：`16 * 1024` bytes

驗收標準：

- 需有獨立指標保存配置結果
- 配置後 Internal 類可用記憶體應出現下降趨勢

#### FR-008 配置目標二：DMA Memory

系統必須配置一塊 DMA 記憶體。

- 指定 capability：`MALLOC_CAP_DMA`
- 建議配置大小：`8 * 1024` bytes

驗收標準：

- 需有獨立指標保存配置結果
- 配置後 DMA 類可用記憶體應反映變化

#### FR-009 配置目標三：PSRAM

系統必須配置一塊 PSRAM 記憶體。

- 指定 capability：`MALLOC_CAP_SPIRAM`
- 建議配置大小：`64 * 1024` bytes

驗收標準：

- 需有獨立指標保存配置結果
- 配置後 PSRAM 可用記憶體應出現下降趨勢

#### FR-010 配置失敗處理

系統必須檢查 `heap_caps_malloc` 回傳值是否為 `nullptr`。

驗收標準：

- 若配置失敗，必須輸出錯誤訊息
- 錯誤訊息至少需包含失敗的記憶體類型
- 程式不得因空指標而直接發生未定義行為

#### FR-011 資源釋放

系統應於觀察完成後釋放已配置之記憶體。

驗收標準：

- 已成功配置之指標應對應呼叫 `free`
- 不得重複釋放同一指標

說明：

- 原始作業文件未硬性要求釋放，但正式規格納入此要求，以確保程式完整性與可重複執行性

### 7.3 Part 3：觀察與比較流程

#### FR-012 配置前狀態輸出

系統必須在進行任何指定配置前，先輸出一次記憶體狀態。

驗收標準：

- Serial Monitor 中需出現明確區段標題
- 該區段必須先於配置動作發生

#### FR-013 配置動作

系統必須依序或明確地完成多種類型記憶體配置。

驗收標準：

- 至少包含 Internal、DMA、PSRAM 三類配置
- 配置流程需可從程式結構中清楚辨識

#### FR-014 配置後狀態輸出

系統必須在配置完成後再次輸出記憶體狀態。

驗收標準：

- 輸出內容結構需與配置前相同或高度一致
- 需能直接對照差異

#### FR-015 差異比較

系統必須能讓審查者從輸出結果判讀前後差異。

驗收標準：

- 至少可觀察 Heap free、Internal free、PSRAM free 的數值變化
- 若有 largest free block，應可輔助說明 fragmentation 現象

## 8. 輸出規格

### 8.1 輸出介面

- 輸出介面：Serial Monitor
- 建議鮮明分段，便於截圖與人工比對

### 8.2 最低輸出格式

系統至少應輸出與下列格式等價之內容：

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

### 8.3 輸出品質要求

- 欄位名稱需清楚
- 前後兩段輸出格式需一致
- 數值不得混用模糊單位
- 錯誤訊息與正常資訊需可區分

## 9. 非功能需求

### 9.1 可讀性

- 程式應具清楚命名
- 記憶體查詢、配置、輸出應分段實作
- 不可將所有邏輯混雜於單一大型區塊而難以維護

### 9.2 可驗證性

- 輸出內容需足以支援人工比對
- 配置前後結果需可重現
- 配置邏輯需能由程式碼直接追溯

### 9.3 穩定性

- 程式不得因配置失敗直接崩潰
- 在具 PSRAM 的目標板上應可穩定完成一次完整流程

## 10. 驗收測試準則

### 10.1 測試案例一：啟動後初始輸出

前置條件：

- 板子成功開機
- Serial Monitor 可接收輸出

預期結果：

- 出現 `=== Before Allocation ===`
- 顯示 Heap、Internal、PSRAM 相關資訊

### 10.2 測試案例二：三類記憶體配置

前置條件：

- `heap_caps_malloc` 可呼叫

預期結果：

- Internal、DMA、PSRAM 三類配置皆被執行
- 成功時指標非空
- 失敗時有對應錯誤訊息

### 10.3 測試案例三：配置後變化

前置條件：

- 已完成三類配置

預期結果：

- 出現 `=== After Allocation ===`
- 至少一項以上記憶體 free size 較配置前下降
- PSRAM 配置成功時，PSRAM free 應下降

### 10.4 測試案例四：報告可分析性

前置條件：

- 已取得 Serial output 截圖

預期結果：

- 可由輸出判讀各類記憶體差異
- 可撰寫配置前後變化分析
- 可觀察是否有 fragmentation 跡象

## 11. 交付物規格

實作者必須提交以下內容：

1. 完整程式碼
2. Serial output 執行截圖
3. 1 至 2 頁簡短報告

報告必須至少包含：

- 各種記憶體差異說明
- 分配後變化分析
- 觀察到的現象，例如 fragmentation

## 12. 評分對應

本規格與來源文件評分項目之對應如下：

| 評分項目 | 比例 | 對應規格重點 |
| ----- | ----- | ----- |
| 記憶體資訊顯示正確 | 30% | FR-001 ~ FR-005 |
| 正確使用 `heap_caps_malloc` | 25% | FR-006 ~ FR-010 |
| 記憶體變化觀察 | 20% | FR-012 ~ FR-015 |
| 程式結構與可讀性 | 15% | 第 9 章 |
| 報告分析 | 10% | 第 11 章 |

## 13. 提交流程要求

依來源文件，提交流程應包含：

1. 從 `nn-speaker` fork 一份 repository
2. 完成 Part 1 至 Part 3
3. 建立 pull request 回原始 repository
4. 繳交 fork repository 的 URL

## 14. 風險與假設

### 14.1 風險

- 若目標板未啟用 PSRAM，`MALLOC_CAP_SPIRAM` 測試可能失敗
- 不同 ESP32 變體的記憶體可用量不同，輸出數值不應寫死
- 若系統背景工作較多，配置前後數值可能有小幅自然波動

### 14.2 假設

- 審查環境具備可用的 Serial Monitor
- 板端已正確設定開發框架與 PSRAM 支援
- 允許使用來源文件列出的建議 API 或其等價實作

## 15. 結論

本規格書將原始作業說明轉換為可實作、可測試、可交付的正式要求。實作者需完成記憶體資訊查詢、指定 capability 配置、配置前後比較，以及對應輸出與報告交付，方可視為符合本作業規格。
