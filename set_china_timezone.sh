#!/bin/bash
# 设置系统时区为中国时间（Asia/Shanghai）

echo "=========================================="
echo "设置系统时区为中国时间（CST, UTC+8）"
echo "=========================================="
echo ""

# 检查是否有sudo权限
if [ "$EUID" -ne 0 ]; then
    echo "❌ 需要管理员权限运行此脚本"
    echo "请使用: sudo bash $0"
    exit 1
fi

# 显示当前时区
echo "当前时区设置:"
timedatectl | grep "Time zone"

echo ""
echo "正在修改时区为 Asia/Shanghai..."

# 设置时区
timedatectl set-timezone Asia/Shanghai

if [ $? -eq 0 ]; then
    echo ""
    echo "✅ 时区修改成功！"
    echo ""
    echo "新时区设置:"
    timedatectl
    echo ""
    echo "当前时间（中国时间）:"
    date
else
    echo ""
    echo "❌ 时区修改失败"
    exit 1
fi

echo ""
echo "=========================================="
echo "时区设置完成"
echo "=========================================="
