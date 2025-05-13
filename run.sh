#!/usr/bin/env bash

# === 路徑設定 ===
# 取得腳本自身所在目錄（不管從哪裡呼叫都正確）
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="${SCRIPT_DIR}"
CMAKE_FILE="${PROJECT_DIR}/CMakeLists.txt"

# 確認 CMakeLists.txt 存在
if [ ! -f "${CMAKE_FILE}" ]; then
    echo "❌ 無法找到 ${CMAKE_FILE}，請確認專案根目錄下有 CMakeLists.txt"
    exit 1
fi

# 從 CMakeLists.txt 裡解析 project 名稱 (第一個參數)
PROJECT_NAME="$(grep -E '^[[:space:]]*project\(' "${CMAKE_FILE}" \
               | head -n1 \
               | sed -E 's/^[[:space:]]*project\(\s*([A-Za-z0-9_]+).*/\1/')"

# 目錄變數
BUILD_DIR="${PROJECT_DIR}/build"
BIN_DIR="${PROJECT_DIR}/bin"

# === 預設值 ===
RUN_TESTS=false

# === 參數解析 ===
for arg in "$@"; do
  case $arg in
    --test)
      RUN_TESTS=true
      shift
      ;;
    *)
      ;;
  esac
done

# === 清理舊的 build 目錄 ===
if [ -d "${BUILD_DIR}" ]; then
  echo "🗑️ 發現已存在的 build 目錄，正在移除..."
  rm -rf "${BUILD_DIR}" || {
    echo "❌ 無法移除 build 目錄！請檢查權限。"
    exit 1
  }
fi

# === 建立 bin 目錄 if needed ===
if [ ! -d "${BIN_DIR}" ]; then
  echo "📁 找不到 bin 目錄，正在建立..."
  mkdir -p "${BIN_DIR}" || {
    echo "❌ 無法建立 bin 目錄！"
    exit 1
  }
else
  echo "📁 已存在 bin 目錄，繼續…"
fi

# === 建置步驟 ===
echo "📦 建立新的 build 目錄: ${BUILD_DIR}"
mkdir -p "${BUILD_DIR}" || {
  echo "❌ 無法建立 build 目錄！"
  exit 1
}
cd "${BUILD_DIR}" || {
  echo "❌ 無法進入 build 目錄！"
  exit 1
}

echo "⚙️ 執行 CMake 配置…"
if [ "${RUN_TESTS}" = false ]; then
  cmake -DBUILD_TESTS=OFF -DLINK_GTEST=OFF .. || {
    echo "❌ CMake 配置失敗！"
    exit 1
  }
else
  echo "✅ 啟用測試模式…"
  cmake -DBUILD_TESTS=ON -DLINK_GTEST=ON .. || {
    echo "❌ CMake 配置失敗！"
    exit 1
  }
fi

echo "🔨 編譯中…"
cmake --build . || {
  echo "❌ 編譯失敗！"
  exit 1
}

echo "✅ 建置完成！"

# === 執行測試 or 主程式 ===
if [ "${RUN_TESTS}" = true ]; then
  echo "🧪 執行單元測試…"
  cd "${BUILD_DIR}/cmake"
  ./run_tests || {
    echo "❌ 測試失敗！"
    exit 1
  }
else
  echo "🚀 執行 ${PROJECT_NAME}…"
  cp "${BUILD_DIR}/cmake/${PROJECT_NAME}" "${BIN_DIR}/${PROJECT_NAME}"
  cd "${BIN_DIR}"
  "./${PROJECT_NAME}"
fi
