# Containerfile (for FinanceRedisProject)

# --- Stage 1: Builder ---
# 使用您上面定義的、已包含預編譯第三方函式庫和設定好環境變數的 Builder Image
# 假設您將上面的 Containerfile.builder 建置成了名為 my_builder_with_env:latest 的映像檔
FROM sinopac.io/cpp-builder:latest AS builder

WORKDIR /app

# 複製您的 FinanceRedisProject 原始碼
COPY . .

# 賦予 run.sh 執行權限
RUN chmod +x ./run.sh

# 執行 run.sh 來編譯您的 FinanceRedisProject
# 您的 run.sh 中的 CMake 現在會透過環境變數 THIRD_PARTY_DIR_ENV
# (或者直接使用 CMakeLists.txt 中讀取環境變數的邏輯)
# 來找到函式庫。您可能不再需要 --third-party-dir 參數傳給 run.sh。
RUN ./run.sh --build-only --third-party-dir /opt/third_party

# --- Stage 2: Runner ---
# ... (與之前類似，從 Builder 的 ${THIRD_PARTY_DIR_ENV}/lib 或 ${DOWNLOAD_TARGET_DIR}/lib 複製 .so 檔案) ...
FROM registry.access.redhat.com/ubi9/ubi:latest
WORKDIR /app
COPY --from=builder /app/bin/finance_stock_quota ./finance_stock_quota
# RUN microdnf install -y libstdc++ shadow-utils && microdnf clean all
# 假設 THIRD_PARTY_DIR_ENV 的值是 /opt/downloaded_libs
RUN chmod +x ./finance_stock_quota
EXPOSE 9516

CMD ["./finance_stock_quota", "--init"]