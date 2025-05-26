#!/usr/bin/env bash

# === 路徑設定 ===
# 取得腳本自身所在目錄（不管從哪裡呼叫都正確）
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="${SCRIPT_DIR}"
CMAKE_FILE="${PROJECT_DIR}/CMakeLists.txt"

# 目錄變數
BUILD_DIR="${PROJECT_DIR}/build"
BIN_DIR="${PROJECT_DIR}/bin"

# === 新增：定義清理函數 ===
cleanup() {
    echo "🧹 執行清理程序..."
    if [ -d "${BUILD_DIR}" ]; then
        echo "🗑️ 正在移除 build 目錄: ${BUILD_DIR}"
        rm -rf "${BUILD_DIR}"
    fi
}

# === 新增：設定陷阱 (trap) ===
# 當腳本因錯誤退出 (EXIT)，或收到中斷 (INT)，終止 (TERM) 信號時，執行 cleanup 函數
trap cleanup EXIT INT TERM

# === Exit on error ===
# 將 set -e 移到 trap 之後，確保 trap 能被正確設定
set -e

# 確認 CMakeLists.txt 存在
if [ ! -f "${CMAKE_FILE}" ]; then
    echo "❌ 無法找到 ${CMAKE_FILE}，請確認專案根目錄下有 CMakeLists.txt"
    exit 1 # 這裡的 exit 會觸發上面設定的 trap
fi

# 從 CMakeLists.txt 裡解析 project 名稱 (第一個參數)
PROJECT_NAME="$(grep -E '^[[:space:]]*project\(' "${CMAKE_FILE}" \
               | head -n1 \
               | sed -E 's/^[[:space:]]*project\(\s*([A-Za-z0-9_]+).*/\1/')"

# === 預設值 ===
RUN_TESTS=false
BUILD_ONLY=false

# === 新增：接收第三方函式庫路徑參數 ===
CUSTOM_THIRD_PARTY_DIR="/Users/ray/cppackage/third_party"

# === 參數解析 ===
while [[ "$#" -gt 0 ]]; do
    case $1 in
        --test) RUN_TESTS=true; shift ;;
        --build-only) BUILD_ONLY=true; shift ;;
        --third-party-dir) CUSTOM_THIRD_PARTY_DIR="$2"; shift 2 ;; # 接收路徑參數
        *) echo "Unknown parameter passed: $1"; exit 1 ;; # 這裡的 exit 會觸發 trap
    esac
done

# === 清理舊的 build 目錄 (這部分可以由 trap 處理，但保留也無妨，trap 會在腳本最終退出時執行) ===
# if [ -d "${BUILD_DIR}" ]; then
#   echo "🗑️ 發現已存在的 build 目錄，正在移除..."
#   rm -rf "${BUILD_DIR}" || {
#     echo "❌ 無法移除 build 目錄！請檢查權限。"
#     exit 1 # 這裡的 exit 會觸發 trap
#   }
# fi

# === 建立 bin 目錄 if needed ===
if [ ! -d "${BIN_DIR}" ]; then
  echo "📁 找不到 bin 目錄，正在建立..."
  mkdir -p "${BIN_DIR}" || {
    echo "❌ 無法建立 bin 目錄！"
    exit 1 # 這裡的 exit 會觸發 trap
  }
else
  echo "📁 已存在 bin 目錄，繼續…"
fi

# === 建置步驟 ===
echo "📦 建立新的 build 目錄: ${BUILD_DIR}"
mkdir -p "${BUILD_DIR}" || {
  echo "❌ 無法建立 build 目錄！"
  exit 1 # 這裡的 exit 會觸發 trap
}
cd "${BUILD_DIR}" || {
  echo "❌ 無法進入 build 目錄！"
  exit 1 # 這裡的 exit 會觸發 trap
}

echo "⚙️ 準備 CMake 配置參數…"
CMAKE_ARGS=() # 初始化 CMake 參數陣列

if [ -n "$CUSTOM_THIRD_PARTY_DIR" ]; then
  echo "🛠️ 使用自訂的第三方函式庫路徑: ${CUSTOM_THIRD_PARTY_DIR}"
  CMAKE_ARGS+=("-DTHIRD_PARTY_DIR=${CUSTOM_THIRD_PARTY_DIR}")
else
  echo "ℹ️ 使用 CMakeLists.txt 中預設的 THIRD_PARTY_DIR"
fi

CMAKE_ARGS+=("-DCMAKE_MODULE_PATH=${PROJECT_DIR}/cmake")

if [ "${RUN_TESTS}" = false ]; then
  CMAKE_ARGS+=("-DBUILD_TESTS=OFF" "-DLINK_GTEST=OFF")
else
  echo "✅ 啟用測試模式…"
  CMAKE_ARGS+=("-DBUILD_TESTS=ON" "-DLINK_GTEST=ON")
fi

echo "⚙️ 執行 CMake 配置…"
cmake "${CMAKE_ARGS[@]}" .. # 如果這裡失敗，set -e 會導致腳本退出，觸發 trap

echo "🔨 編譯中…"
cmake --build . # 如果這裡失敗，set -e 會導致腳本退出，觸發 trap

echo "✅ 建置完成！"

if [ "${RUN_TESTS}" = true ]; then
  echo "🧪 執行單元測試…"
  cd "${PROJECT_DIR}/build/cmake"
  ./run_tests # 如果這裡失敗，set -e 會導致腳本退出，觸發 trap
fi

echo "🚀 將 ${PROJECT_NAME} 複製到 ${BIN_DIR}..."
cp "${PROJECT_DIR}/build/cmake/${PROJECT_NAME}" "${BIN_DIR}/${PROJECT_NAME}" # 如果這裡失敗，set -e 會導致腳本退出，觸發 trap

echo "✅ 執行檔已複製到 ${BIN_DIR}"

# 腳本成功執行到這裡時，我們不希望 trap 在正常退出時也刪除 build 目錄
# 所以在 --build-only 模式或正常執行完主程式後，明確地移除 trap 或以成功狀態退出
if [ "${BUILD_ONLY}" = true ]; then
  echo "✅ 建置完成 (--build-only 模式)！"
  # 在 build-only 模式下，我們通常希望保留 build 目錄供檢查
  # 如果您仍然希望在此模式下也刪除 build，則不需要下面的 trap - EXIT
  # 或者，如果您希望 build-only 模式下保留 build，則可以在這裡取消 EXIT trap
  # trap - EXIT # 取消 EXIT trap，這樣 build 目錄不會被刪除
  exit 0 # 正常退出
fi

# 執行主程式
echo "🚀 執行主程式..."
cd "${BIN_DIR}"
"./${PROJECT_NAME}" # 如果這裡失敗，set -e 會導致腳本退出，觸發 trap

echo "✅ 完成 run.sh ！"
trap - EXIT # 成功執行完畢，取消 EXIT trap，避免刪除 build 目錄
exit 0