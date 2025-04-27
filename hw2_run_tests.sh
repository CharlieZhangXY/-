#!/bin/bash

# 檔案大小（MB）
file_sizes=(50 100 200)

# AIO 併發區塊數
aio_parts=(2 4 8)

# 可執行檔
EXE="./hw2"
RESULT_CSV="hw2_results.csv"

# 編譯檢查
if [ ! -f "$EXE" ]; then
  echo "❌ 找不到 $EXE，請先執行：gcc -o hw2 hw2.c -lrt"
  exit 1
fi

# 初始化 CSV 檔案
echo "FileSize_MB,AIO_Blocks,Blocking_Time_ms,AIO_Time_ms" > $RESULT_CSV

# 執行測試
for size in "${file_sizes[@]}"; do
  echo "📂 產生 input.txt ${size}MB..."
  dd if=/dev/urandom of=input.txt bs=1M count=$size status=none

  for part in "${aio_parts[@]}"; do
    echo "🚀 測試：${size}MB / AIO區塊數=${part}"

    # 執行並擷取時間
    output=$(sudo $EXE $part)

    block_time=$(echo "$output" | grep "Blocking" -A1 | tail -1 | grep -oP "\\d+\\.\\d+")
    aio_time=$(echo "$output" | grep "AIO" -A1 | tail -1 | grep -oP "\\d+\\.\\d+")

    # 顯示與記錄結果
    echo "🔸 Blocking I/O Time: $block_time ms"
    echo "🔸 AIO Time:          $aio_time ms"
    echo "${size},${part},${block_time},${aio_time}" >> $RESULT_CSV
    echo "------------------------------------------------------"
  done
done

echo ""
echo "✅ 所有測試完成，結果已輸出至：$RESULT_CSV"
