#!/bin/bash

# 測試檔案大小（MB）
sizes=(10 100 500 1000)

# AIO 並行區塊數（等分切割）
aio_parts=(4 8 16)

# 可執行檔
EXE="./hw1"

# 編譯檢查
if [ ! -f "$EXE" ]; then
  echo "❌ 找不到執行檔 $EXE，請先執行：gcc -o hw1 hw1.c -lrt"
  exit 1
fi

# 測試所有組合
for size in "${sizes[@]}"; do
  echo ""
  echo "🔧 建立 ${size}MB 測試檔案..."
  dd if=/dev/urandom of=input.txt bs=1M count=$size status=none

  for part in "${aio_parts[@]}"; do
    echo ""
    echo "🚀 測試組合：檔案=${size}MB，切割區塊=${part}"
    sudo $EXE $part
    echo "-----------------------------------------"
  done
done

echo ""
echo "✅ 所有測試完成"
