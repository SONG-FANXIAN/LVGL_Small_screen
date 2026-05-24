# FreeRTOS 快速参考指南

## 🎯 核心概念

### 任务 (Task)

独立执行的代码段，拥有自己的堆栈和上下文。

```cpp
void myTask(void *pvParameters) {
    while (1) {
        // 任务代码
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
```

### 优先级 (Priority)

数字越大 = 优先级越高。0为最低（Idle任务）。

### 任务状态

```
┌─ Ready ──┐
│   (就绪) │
│          ↓
├─ Running ─ (运行)
│          ↑
│   (就绪) │
└─ Blocked ┘  (阻塞 - 等待事件或延迟)
```

## 📋 常用API速查表

### 任务管理

| 函数                        | 功能                     | 示例                                                   |
| --------------------------- | ------------------------ | ------------------------------------------------------ |
| `xTaskCreate()`             | 创建任务                 | `xTaskCreate(myTask, "task", 2048, NULL, 1, &handle);` |
| `xTaskCreatePinnedToCore()` | 创建任务并绑定到指定核心 | `xTaskCreatePinnedToCore(myTask, ..., 1, &handle, 0);` |
| `vTaskDelete()`             | 删除任务                 | `vTaskDelete(handle);`                                 |
| `vTaskDelay()`              | 延迟（释放CPU）          | `vTaskDelay(pdMS_TO_TICKS(100));`                      |
| `vTaskPrioritySet()`        | 改变优先级               | `vTaskPrioritySet(handle, 2);`                         |
| `uxTaskPriorityGet()`       | 获取优先级               | `uint8_t p = uxTaskPriorityGet(handle);`               |
| `uxTaskGetNumberOfTasks()`  | 获取任务数               | `uint32_t n = uxTaskGetNumberOfTasks();`               |

### 延迟和时间

| 函数                  | 功能             |
| --------------------- | ---------------- |
| `vTaskDelay()`        | 延迟指定Tick数   |
| `pdMS_TO_TICKS()`     | 毫秒转换为Tick数 |
| `vTaskDelayUntil()`   | 延迟到指定时刻   |
| `xTaskGetTickCount()` | 获取当前Tick数   |

### 消息队列

| 函数              | 功能             | 示例                                               |
| ----------------- | ---------------- | -------------------------------------------------- |
| `xQueueCreate()`  | 创建队列         | `QueueHandle_t q = xQueueCreate(10, sizeof(int));` |
| `xQueueSend()`    | 发送消息         | `xQueueSend(queue, &data, pdMS_TO_TICKS(100));`    |
| `xQueueReceive()` | 接收消息         | `xQueueReceive(queue, &data, pdMS_TO_TICKS(100));` |
| `xQueuePeek()`    | 查看消息(不删除) | `xQueuePeek(queue, &data, 0);`                     |

### 互斥锁 (Semaphore)

| 函数                      | 功能       | 示例                                             |
| ------------------------- | ---------- | ------------------------------------------------ |
| `xSemaphoreCreateMutex()` | 创建互斥锁 | `SemaphoreHandle_t m = xSemaphoreCreateMutex();` |
| `xSemaphoreTake()`        | 获取锁     | `xSemaphoreTake(mutex, pdMS_TO_TICKS(100));`     |
| `xSemaphoreGive()`        | 释放锁     | `xSemaphoreGive(mutex);`                         |

### 其他

| 函数                            | 功能                      |
| ------------------------------- | ------------------------- |
| `xPortGetCoreID()`              | 获取当前运行核心ID (0或1) |
| `portNUM_PROCESSORS`            | ESP32核心数 (2)           |
| `eTaskGetState()`               | 获取任务状态              |
| `uxTaskGetStackHighWaterMark()` | 获取堆栈最大使用量        |

## 🔧 常见代码片段

### 创建任务

```cpp
// 基础创建
xTaskCreate(
    myTask,                 // 任务函数
    "MyTask",               // 任务名称（调试用）
    2048,                   // 堆栈大小（字节）
    NULL,                   // 传递给任务的参数
    1,                      // 优先级 (0最低, 3最高)
    &myTaskHandle           // 任务句柄（指针）
);

// 绑定到指定核心（推荐用于ESP32）
xTaskCreatePinnedToCore(
    myTask,                 // 任务函数
    "MyTask",               // 任务名称
    2048,                   // 堆栈大小
    NULL,                   // 参数
    1,                      // 优先级
    &myTaskHandle,          // 任务句柄
    0                       // 核心ID: 0或1
);
```

### 延迟和循环

```cpp
void myTask(void *pvParameters) {
    while (1) {
        Serial.println("Hello");

        // 延迟1秒（会释放CPU给其他任务）
        vTaskDelay(pdMS_TO_TICKS(1000));

        // 错误：不要使用delay()！
        // delay(1000);  // ❌ 会阻塞所有任务
    }
}
```

### 消息队列通信

```cpp
// 发送方任务
void senderTask(void *pvParameters) {
    int data = 42;

    while (1) {
        xQueueSend(
            myQueue,                    // 队列句柄
            &data,                      // 数据指针
            pdMS_TO_TICKS(100)          // 超时时间
        );

        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

// 接收方任务
void receiverTask(void *pvParameters) {
    int data;

    while (1) {
        if (xQueueReceive(
            myQueue,                    // 队列句柄
            &data,                      // 接收数据的指针
            pdMS_TO_TICKS(1000)         // 超时时间
        ) == pdTRUE) {
            Serial.printf("收到数据: %d\n", data);
        }
    }
}
```

### 互斥锁保护

```cpp
// 全局变量
SemaphoreHandle_t myMutex;
int sharedVariable = 0;

// 初始化
void setup() {
    myMutex = xSemaphoreCreateMutex();
}

// 任务中使用
void task1(void *pvParameters) {
    while (1) {
        // 尝试获取锁，100ms超时
        if (xSemaphoreTake(myMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
            // 临界区 - 独占访问
            sharedVariable++;
            Serial.printf("task1: %d\n", sharedVariable);

            // 释放锁
            xSemaphoreGive(myMutex);
        } else {
            Serial.println("获取锁失败（超时）");
        }

        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
```

### 监控任务状态

```cpp
void monitorTask(void *pvParameters) {
    while (1) {
        Serial.println("\n=== 任务监控 ===");
        Serial.printf("总任务数: %u\n", uxTaskGetNumberOfTasks());

        // 显示核心信息
        Serial.printf("ESP32核心数: %d\n", portNUM_PROCESSORS);
        Serial.printf("当前核心: %d\n", xPortGetCoreID());

        // 显示堆信息
        Serial.printf("总堆内存: %u 字节\n", ESP.getHeapSize());
        Serial.printf("剩余堆内存: %u 字节\n", ESP.getFreeHeap());

        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}
```

## 🎮 实战示例

### 示例1：双任务通信

```cpp
QueueHandle_t dataQueue;

void producerTask(void *pvParameters) {
    int counter = 0;
    while (1) {
        xQueueSend(dataQueue, &counter, pdMS_TO_TICKS(100));
        Serial.printf("生产: %d\n", counter++);
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}

void consumerTask(void *pvParameters) {
    int data;
    while (1) {
        if (xQueueReceive(dataQueue, &data, pdMS_TO_TICKS(1000)) == pdTRUE) {
            Serial.printf("消费: %d\n", data);
        }
    }
}

void setup() {
    dataQueue = xQueueCreate(10, sizeof(int));

    xTaskCreatePinnedToCore(producerTask, "Producer", 2048, NULL, 2, NULL, 0);
    xTaskCreatePinnedToCore(consumerTask, "Consumer", 2048, NULL, 1, NULL, 1);
}
```

### 示例2：互斥锁保护共享资源

```cpp
SemaphoreHandle_t dataLock;
int sharedCounter = 0;

void incrementTask(void *pvParameters) {
    while (1) {
        if (xSemaphoreTake(dataLock, pdMS_TO_TICKS(100)) == pdTRUE) {
            int temp = sharedCounter;
            vTaskDelay(pdMS_TO_TICKS(10));  // 模拟处理
            sharedCounter = temp + 1;

            Serial.printf("递增: %d\n", sharedCounter);
            xSemaphoreGive(dataLock);
        }
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

void setup() {
    dataLock = xSemaphoreCreateMutex();
    xTaskCreatePinnedToCore(incrementTask, "Inc1", 2048, NULL, 2, NULL, 0);
    xTaskCreatePinnedToCore(incrementTask, "Inc2", 2048, NULL, 2, NULL, 1);
}
```

## ⚠️ 常见错误

### ❌ 错误1：使用delay()而不是vTaskDelay()

```cpp
// 错误
while (1) {
    delay(1000);  // ❌ 阻塞所有任务！
}

// 正确
while (1) {
    vTaskDelay(pdMS_TO_TICKS(1000));  // ✓ 释放CPU给其他任务
}
```

### ❌ 错误2：堆栈大小太小

```cpp
// 错误 - 堆栈太小，可能溢出
xTaskCreate(myTask, "task", 256, NULL, 1, NULL);

// 正确 - 一般需要2KB以上
xTaskCreate(myTask, "task", 2048, NULL, 1, NULL);
```

### ❌ 错误3：不保护共享资源

```cpp
// 错误 - 竞态条件
int counter = 0;
task1: counter++;  // 读-改-写
task2: counter++;

// 正确 - 使用互斥锁
if (xSemaphoreTake(mutex, ...) == pdTRUE) {
    counter++;
    xSemaphoreGive(mutex);
}
```

### ❌ 错误4：优先级反转

```cpp
// 错误 - 高优先级任务等待低优先级任务释放锁
// 中优先级任务抢占，导致死锁

// 正确 - 使用优先级继承互斥锁
xSemaphoreCreateMutex();  // 自动支持优先级继承
```

## 💡 调试技巧

### 打印任务信息

```cpp
void printTaskInfo() {
    Serial.println("任务列表:");
    Serial.println("优先级 | 状态 | 堆栈 | 名称");
    Serial.println("-------|------|------|--------");

    // FreeRTOS没有内置函数列举所有任务
    // 需要自己跟踪任务句柄
}
```

### 监控堆栈使用

```cpp
UBaseType_t watermark = uxTaskGetStackHighWaterMark(myTaskHandle);
Serial.printf("堆栈最大使用: %u字节\n", watermark);
Serial.printf("堆栈剩余: %u字节\n", (4096 - watermark));
```

### 获取任务状态

```cpp
eTaskState state = eTaskGetState(myTaskHandle);
switch (state) {
    case eRunning: Serial.println("运行中"); break;
    case eReady: Serial.println("就绪"); break;
    case eBlocked: Serial.println("阻塞"); break;
    case eSuspended: Serial.println("暂停"); break;
    case eDeleted: Serial.println("已删除"); break;
    default: Serial.println("未知"); break;
}
```

## 📚 项目中的使用

本项目中包含3个任务：

```
OTA_Task (优先级3)
├─ 处理蓝牙数据
├─ 发送OTA状态到队列
└─ 核心1运行

Monitor_Task (优先级2)
├─ 接收OTA状态
├─ 显示进度信息
└─ 核心0运行

App_Task (优先级1)
├─ 执行应用业务逻辑
└─ 核心1运行
```

## 🔗 相关文件

- [完整RTOS架构文档](./RTOS_ARCHITECTURE.md)
- [FreeRTOS官方文档](https://www.freertos.org/documentation)
- [ESP32 RTOS示例](https://github.com/espressif/esp-idf/tree/master/examples)

---

**记住**：

- ✅ 使用`vTaskDelay()`而不是`delay()`
- ✅ 合理分配任务优先级
- ✅ 保护共享资源（互斥锁）
- ✅ 监控堆栈和内存使用
- ✅ 充分利用双核优势

Happy coding! 🚀
