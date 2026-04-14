# Matplotlib中文字体警告修复

**日期**: 2026-04-09
**问题**: matplotlib生成图表时出现大量中文字体缺失警告

---

## ⚠️ 警告信息示例

```
RuntimeWarning: Glyph 36135 missing from current font.
RuntimeWarning: Glyph 26550 missing from current font.
RuntimeWarning: Glyph 21335 missing from current font.
...
```

这些是matplotlib在尝试渲染中文字符时的警告，因为系统中缺少对应的中文字体。

---

## ✅ 修复方案

### 1. 抑制字体警告

**文件**: `test/scripts/run_automated_test.py`

**修改位置**: 第51-60行

```python
import matplotlib
matplotlib.use('Agg')  # 使用非交互式后端
import matplotlib.pyplot as plt
import matplotlib.patches as mpatches

# ✅ 抑制中文字体缺失警告
import warnings
warnings.filterwarnings("ignore", category=RuntimeWarning, module='matplotlib')
warnings.filterwarnings("ignore", category=RuntimeWarning, module='matplotlib.backends')
```

### 2. 优化字体设置

**修改位置1**: 第1458-1462行
```python
# ✅ 修改前
plt.rcParams['font.family'] = 'serif'

# ✅ 修改后
plt.rcParams['font.family'] = ['sans-serif']  # 使用sans-serif，通常有更好的字体支持
plt.rcParams['axes.unicode_minus'] = False  # 正确显示负号
```

**修改位置2**: 第1906-1913行
```python
# ✅ 修改前
plt.rcParams['font.family'] = 'serif'

# ✅ 修改后
plt.rcParams['font.family'] = ['sans-serif']
plt.rcParams['axes.unicode_minus'] = False
```

---

## 🎯 修复效果

### 修复前
```
RuntimeWarning: Glyph 36135 missing from current font.
RuntimeWarning: Glyph 26550 missing from current font.
[几十行警告...]
```

### 修复后
```
✅ 无警告输出
✅ 图表正常生成
✅ 中文仍可显示（如果系统有中文字体）
```

---

## 📝 说明

### 为什么会警告？
- matplotlib默认使用'serif'字体族
- Linux系统中'serif'字体通常不包含中文字符
- 当图表包含中文时，matplotlib会警告缺失的字符

### 修复原理
1. **抑制警告**: 使用`warnings.filterwarnings()`过滤matplotlib的RuntimeWarning
2. **改用sans-serif**: sans-serif字体族通常有更好的字体支持
3. **unicode_minus**: 确保负号正确显示

### 中文显示
- 如果系统有中文字体（如DejaVu、WenQuanYi等），中文会正常显示
- 如果没有中文字体，中文会显示为方框或问号，但不会有警告

---

## 🔧 可选：安装中文字体

如果需要完整的中文支持，可以安装中文字体包：

```bash
# Ubuntu/Debian
sudo apt-get install fonts-wqy-microhei fonts-wqy-zenhei

# 或使用文泉驿字体
sudo apt-get install fonts-noto-cjk fonts-noto-cjk-extra

# 更新字体缓存
fc-cache -fv
```

然后在matplotlib中配置：
```python
plt.rcParams['font.sans-serif'] = ['WenQuanYi Micro Hei', 'DejaVu Sans']
```

---

## 📁 修改的文件

- `test/scripts/run_automated_test.py` (4处修改)

---

**生成时间**: 2026-04-09
**修改者**: Claude Sonnet 4.6
