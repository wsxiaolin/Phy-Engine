# 元件补齐计划（revert `e116a91` 之后）

HEAD 已回到 `eaeb072` 的映射策略：Web ModelID 只别名到**工厂已经能 `add_model` 的码**。禁止再发明 302–329 这类空枚举。

每步做完立刻 commit。一次只动同类型、1–3 个元件。提交必须同时包含：模型（若新建）+ `dll_main.cpp` `add_model_via_code` + `pe_sim.h` 映射/展开 + 一条冒烟测试。

## 0. 先修编译（单独一笔）

`pe_sim.h` 里 `Transistor` 对 `status_or` 调了不存在的 `value_or`。改成失败时用默认值：

```cpp
auto is_r = get_required_float_ec(element_data, "Is");
double is_val = is_r ? *is_r.value : 1e-14;
```

`expected_prop_arity_ec(PHY_ENGINE_E_DIGITAL_SCHMITT_TRIGGER)` 现在返回 4，工厂 `case 233` 吃 5 个属性。改成 5，与 `dll_main.cpp` 一致。

commit：`fix: Transistor defaults and Schmitt prop arity`

## 1. 接上已有模型、工厂已实现的码

这些码在 `dll_main.cpp` 里已经有 case，只缺 `to_phy_engine_code_and_props_ec` / `build_()`。

### 1.1 Comparator（code 19）

PE 模型：`model::comparator`，pin `A,B,o`。属性 `Ll,Hl`。

在 `to_phy_engine_code_and_props_ec` 加 `ModelID == "Comparator"` → `PHY_ENGINE_E_COMPARATOR`。属性键缺省时用 0 / 5，走上面的 `status_or` 写法。

测试：两个电压源进 A/B，读输出脚高低。

commit：`feat: map Comparator to PHY_ENGINE_E_COMPARATOR`

### 1.2 Schmitt Trigger（code 233）

PE 模型：`model::SCHMITT_TRIGGER`，pin `i,o`。属性 `Vth_low,Vth_high,inverted,Ll,Hl`。

`ModelID == "Schmitt Trigger"` → `PHY_ENGINE_E_DIGITAL_SCHMITT_TRIGGER`。catalog 2 pin，1:1。

测试：斜坡过阈值，输出翻转一次。

commit：`feat: map Schmitt Trigger to SCHMITT_TRIGGER`

### 1.3 8bit Input / Display（code 231 / 232）

PE 已有 `EIGHT_BIT_INPUT` / `EIGHT_BIT_DISPLAY`，pin `b7..b0`。catalog 各 8 pin。

从 `to_phy_engine_code_and_props_ec` 的 “should be expanded” 名单里拿掉这两个。在 `build_()` 里 1:1 加元件（仿 Counter）：

- Input：属性 `"值"` → `value`（0..255），缺省 0
- Display：无属性

PL pin `0..7` → PE pin `0..7`。先核对 catalog pin 序是否 MSB-first；若 LSB-first，只在 `pin_map` 里翻转，不要改模型。

测试：Input=0xA5，Display 采样 8 脚。

commit：`feat: wire 8bit Input and Display into pe_sim`

## 2. 工厂缺 case 的已有枚举

`dll_api.h` 已有 `PHY_ENGINE_E_SWITCH_SPDT=24`、`PHY_ENGINE_E_SWITCH_DPDT=25`，`add_model_via_code` 没有 case，模型库也只有 `single_pole_switch`（2 pin）。

### 2.1 SPDT 模型 + 工厂

新建 `include/phy_engine/model/models/controller/spdt_switch.h`，照 `switch.h` 的 DC stamp：

- pin：`COM, NO, NC`（3）
- 属性：`cut_through`（true=COM–NO，false=COM–NC）
- 两条 branch，开路用 `mna.r_open`

`dll_main.cpp` `case 24`：读一个 double，构造 SPDT。

`pe_sim.h` 已把 `SPDT Switch` 指到 24，核对 catalog 3 pin 与 COM/NO/NC 顺序后写 `pin_map`。

测试：COM 接电源，切状态后 NO/NC 电压对调。

commit：`feat: add SPDT switch model and factory case 24`

### 2.2 DPDT 模型 + 工厂

DPDT = 两组 SPDT 共用一个 `cut_through`。6 pin：`COM1,NO1,NC1,COM2,NO2,NC2`。catalog 顺序对照 Unity prefab 后再写 `pin_map`。

`dll_main.cpp` `case 25`。

测试：两路独立电源，切一次两路同时换向。

commit：`feat: add DPDT switch model and factory case 25`

## 3. 多脚器件：别名不够，要展开

这些在 `eaeb072` 里被压成 2 脚电阻/电感/二极管，pin 数对不上，仿真拓扑是错的。每个 commit 只修一种。

### 3.1 Analog Joystick（6 pin）

catalog：`X1,X2,X3,Y1,Y2,Y3`。用两个电阻分压（X2/Y2 为抽头），属性 `"电阻"` 为每轴总阻。

在 `build_()` 展开成 4 个 `PHY_ENGINE_E_RESISTOR`（X 上/下、Y 上/下），内部连到中点。不要新枚举。

测试：X2/Y2 对地电压约为电源一半。

commit：`feat: expand Analog Joystick into two voltage dividers`

### 3.2 Slide Rheostat（4 pin）

catalog 4 pin。展开成两个串联电阻，抽头比先读属性 `"滑块"` / `"比例"`，没有就 0.5。总阻走 `"电阻"`。

测试：两端加电压，抽头为 0.5 倍。

commit：`feat: expand Slide Rheostat into two series resistors`

### 3.3 Dual Light-Emitting Diode

catalog 只有 2 pin，先保持 `PN_JUNCTION` 1:1。若 Unity 语义是反向并联，再在 `build_()` 展开成两个极性相反的 `PN_JUNCTION`。确认语义后再动。

commit（仅当确认反向并联）：`feat: expand Dual LED as anti-parallel diodes`

## 4. 555 Timer

引擎没有 555 模型。两种做法，选工作量小的：

- A. `build_()` 用已有比较器 + 放电开关 + 电阻网络搭标准 555（引脚多，单独开一个 commit 序列）
- B. `PHY_ENGINE_E_VERILOG_NETLIST` 塞一份 555 Verilog，只走 `create_circuit_ex`

先核 catalog/Unity 的 pin 名（当前 catalog `pins: []`，布局在 ChipComponent）。没核对前不要写模型。

commit 拆开：`feat: 555 pin map` → `feat: 555 analog core` → `feat: 555 smoke astable`

## 5. 电表 / 传感器 / 灯：先别升级模型

`eaeb072` 把它们别名成 `RESISTOR` / `VDC` / `PN_JUNCTION` / `INDUCTOR`，2 脚器件这样就能进仿真。真电流表内阻、电压表高阻、传感器物理量，等 SPDT/8bit/Joystick 稳定后再单开。

升级时同样一两种一 commit，例如：

- `feat: voltmeter as 1e6 ohm default when 电阻 missing`
- `feat: photodiode as IDC instead of PN_JUNCTION`（先对 Unity 属性键）

## 6. 每笔提交的检查

```bash
# 至少编过并跑相关冒烟（路径随 CMake 目标名）
cmake --build build --target pl_pe_sim_smoke
ctest --output-on-failure -R 'pl_pe|dll_main_model'
```

新工厂码必须加进 `test/0008.dll/dll_main_model_coverage.cpp` 的 `elements[]` / `props[]`。

`documents/docu.zh_CN/05_模型库清单.md` 只在该元件已经能 `create_circuit` 之后改，不要提前写。

## 不要做

- 不要再加没有 `add_model_via_code` case 的 `PHY_ENGINE_E_*`
- 不要在 `build_()` 里 `return code_and_props`
- 不要一次映射几十个 ModelID
- 不要把 4/6 脚器件继续 1:1 塞进 2 脚电阻
