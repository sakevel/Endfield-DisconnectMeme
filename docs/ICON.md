# 图标设计与导出说明

本文档说明 Endfield-DisconnectMeme 的图标规范与导出流程。

---

## 图标规范

- **模组图标** (`mod/icon.png`)：
  256×256 透明 RGBA PNG。
  采用深灰六边形工业徽章底座，中心为断开插头的白色几何轮廓，辅以 `#ffef00` 亮黄断线点缀，直观表现断开连接与演出主题。

---

## 导出流程

从高分辨率源图生成发布图标：

```powershell
./tools/export-icon.ps1 -Source assets/icon-source.png -Destination mod/icon.png
```
