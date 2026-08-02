#include "settings.h"

#include "stm32f10x_flash.h"

/* 参数存储模块：使用STM32最后一页Flash保存标定和用户设置。 */
#define SETTINGS_FLASH_ADDRESS  0x0800FC00UL
#define SETTINGS_MAGIC          0x48583731UL
#define SETTINGS_VERSION        1U

/* Flash记录包含头部、业务参数和校验值，可识别空白页或旧版本数据。 */
typedef struct {
    uint32_t magic;
    uint16_t version;
    uint16_t size;
    App_Settings value;
    uint32_t checksum;
} Settings_Record;

static uint32_t Settings_Checksum(const uint8_t *data, uint32_t length)
{
    /* FNV-1a校验可检测Flash未写入、部分损坏和结构体内容变化。 */
    uint32_t hash = 2166136261UL;
    uint32_t i;

    for (i = 0U; i < length; ++i) {
        hash = (hash ^ data[i]) * 16777619UL;
    }
    return hash;
}

void Settings_SetDefaults(App_Settings *settings)
{
    settings->offset = 0;
    settings->counts_per_gram = 0.0f;
    settings->alarm_limit_g = 1000.0f;
    settings->unit = 0U;
    settings->calibrated = 0U;
}

uint8_t Settings_Load(App_Settings *settings)
{
    const Settings_Record *record = (const Settings_Record *)SETTINGS_FLASH_ADDRESS;
    uint32_t checksum;

    /* 魔数、版本和结构体大小任一不符，都回退到出厂参数。 */
    if ((record->magic != SETTINGS_MAGIC) ||
        (record->version != SETTINGS_VERSION) ||
        (record->size != sizeof(App_Settings))) {
        Settings_SetDefaults(settings);
        return 0U;
    }
    checksum = Settings_Checksum((const uint8_t *)&record->value,
                                 sizeof(record->value));
    if (checksum != record->checksum) {
        Settings_SetDefaults(settings);
        return 0U;
    }
    *settings = record->value;
    return 1U;
}

uint8_t Settings_Save(const App_Settings *settings)
{
    Settings_Record record;
    const uint16_t *words;
    uint32_t address;
    uint32_t i;
    FLASH_Status status = FLASH_COMPLETE;

    record.magic = SETTINGS_MAGIC;
    record.version = SETTINGS_VERSION;
    record.size = sizeof(App_Settings);
    record.value = *settings;
    record.checksum = Settings_Checksum((const uint8_t *)&record.value,
                                        sizeof(record.value));
    if ((sizeof(record) & 1U) != 0U) {
        return 0U;
    }
    words = (const uint16_t *)&record;

    /* STM32F1 Flash只能半字写入，保存前必须整页擦除。 */
    FLASH_Unlock();
    status = FLASH_ErasePage(SETTINGS_FLASH_ADDRESS);
    address = SETTINGS_FLASH_ADDRESS;
    for (i = 0U; (i < sizeof(record) / 2U) && (status == FLASH_COMPLETE); ++i) {
        status = FLASH_ProgramHalfWord(address, words[i]);
        address += 2U;
    }
    FLASH_Lock();
    return (status == FLASH_COMPLETE) ? 1U : 0U;
}
