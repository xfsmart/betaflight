# FT32F4 I2C 驱动迁移说明

**日期**: 2026-03-16  
**操作人**: 二牛 🐮

---

## 一、HAL 库驱动文件已迁移

### 1.1 迁移目录

```
/home/administrator/betaflight/src/platform/FT32/drivers_hal_legacy/
```

### 1.2 迁移文件

| 文件 | 说明 | 状态 |
|------|------|------|
| `bus_i2c_ft32.c` | I2C 总线驱动实现 (HAL 库) | ✅ 已迁移 |
| `bus_i2c_ft32_init.c` | I2C 初始化配置 (HAL 库) | ✅ 已迁移 |
| `io_ft32.c` | IO 端口配置 (HAL 库 GPIO) | ✅ 已迁移 |

### 1.3 原目录结构

```
/home/administrator/betaflight/src/platform/FT32/
├── drivers_hal_legacy/          # ← HAL 库驱动 (已迁移至此)
│   ├── bus_i2c_ft32.c
│   ├── bus_i2c_ft32_init.c
│   └── io_ft32.c
├── include/
│   └── platform/
│       └── platform.h
├── link/
├── mk/
├── startup/
└── target/
    └── FT32F405/
        └── target.h
```

---

## 二、HAL 库驱动问题分析

### 2.1 未完成的移植版本

**文件**: `FT32F4xx_HAL_Driver/Src/ft32f4xx_hal_i2c.c`

**缺失函数**: 17 个

| 类别 | 函数数量 | 状态 |
|------|---------|------|
| DMA 回调函数 | 6 个 | ❌ 只有声明 |
| IT 中断处理函数 | 7 个 | ❌ 只有声明 |
| ISR 中断服务函数 | 4 个 | ❌ 只有声明 |

### 2.2 当前可用功能

✅ **已实现** (轮询模式):
- I2C_Init / I2C_DeInit
- I2C_Master_Transmit / Receive
- I2C_Mem_Write / Read
- I2C_IsDeviceReady

❌ **缺失** (中断/DMA 模式):
- 所有 DMA 相关函数
- 所有中断处理函数
- 所有 ISR 服务函数

---

## 三、FT32F4xx 标准库资源

### 3.1 标准库位置

```
/home/administrator/betaflight/lib/main/FT32F4/Drivers/FT32F4xx_Driver/
├── Inc/           # 头文件
│   ├── ft32f4xx_i2c.h      # I2C 标准库接口
│   ├── ft32f4xx_gpio.h     # GPIO 标准库接口
│   ├── ft32f4xx_rcc.h      # RCC 标准库接口
│   └── ...
└── Src/           # 源文件
    ├── ft32f4xx_i2c.c      # I2C 标准库实现
    ├── ft32f4xx_gpio.c     # GPIO 标准库实现
    └── ...
```

### 3.2 标准库 I2C 接口

```c
// 初始化结构体
typedef struct {
  uint32_t I2C_Timing;
  uint32_t I2C_AnalogFilter;
  uint32_t I2C_DigitalFilter;
  uint32_t I2C_NoStretchMode;
  uint32_t I2C_Mode;
  uint32_t I2C_OwnAddress1;
  uint32_t I2C_Ack;
  uint32_t I2C_AcknowledgedAddress;
} I2C_InitTypeDef;

// 标准库函数
void I2C_DeInit(I2C_TypeDef* I2Cx);
FT_StatusTypeDef I2C_Init(I2C_TypeDef* I2Cx, I2C_InitTypeDef* I2C_InitStruct);
FT_StatusTypeDef I2C_Master_Transmit(...);
FT_StatusTypeDef I2C_Master_Receive(...);
FT_StatusTypeDef I2C_Mem_Write(...);
FT_StatusTypeDef I2C_Mem_Read(...);
```

---

## 四、下一步计划

### 4.1 使用标准库重写 I2C 驱动

**目标**: 用 FT32F4xx 标准库实现 Betaflight I2C platform 驱动

**文件位置**:
```
/home/administrator/betaflight/src/platform/FT32/
├── bus_i2c_ft32_std.c          # 标准库版本 I2C 驱动
├── bus_i2c_ft32_init_std.c     # 标准库版本初始化
└── io_ft32_std.c               # 标准库版本 IO 配置
```

### 4.2 优势

1. **代码完整** - 标准库函数都有完整实现
2. **无需 HAL** - 不依赖未完成的 HAL 库
3. **编译通过** - 避免 DMA_HandleTypeDef 未定义问题
4. **易于维护** - 标准库接口稳定

---

## 五、编译链说明

### 5.1 需要添加的源文件

```makefile
# FT32F4xx 标准库源文件
FT32_LIB_SRC = \
    lib/main/FT32F4/Drivers/FT32F4xx_Driver/Src/ft32f4xx_i2c.c \
    lib/main/FT32F4/Drivers/FT32F4xx_Driver/Src/ft32f4xx_gpio.c \
    lib/main/FT32F4/Drivers/FT32F4xx_Driver/Src/ft32f4xx_rcc.c
```

### 5.2 需要添加的包含路径

```makefile
FT32_INCLUDES = \
    -Ilib/main/FT32F4/Drivers/FT32F4xx_Driver/Inc \
    -Ilib/main/FT32F4/CMSIS/cm4/device_support
```

---

*二牛整理 - 2026-03-16*
