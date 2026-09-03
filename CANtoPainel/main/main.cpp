#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stddef.h>

#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"


// ============================================================
// CONFIGURAÇÃO DO DISPLAY TFT
// ============================================================

#define TFT_SCLK GPIO_NUM_12
#define TFT_MOSI GPIO_NUM_11
#define TFT_CS   GPIO_NUM_10
#define TFT_DC   GPIO_NUM_9
#define TFT_RST  GPIO_NUM_46

#define TFT_WIDTH  128
#define TFT_HEIGHT 160

// ============================================================
// VARIÁVEIS GLOBAIS
// ============================================================
static spi_device_handle_t tft;
static uint8_t line_buffer[TFT_WIDTH * 2];

// ============================================================
// ENVIA UM COMANDO PARA O DISPLAY
// ===========================================================

static void tft_command(uint8_t command)
{
    spi_transaction_t transaction = {};

    transaction.length = 8;
    transaction.tx_buffer = &command;

    gpio_set_level(TFT_DC, 0);

    spi_device_transmit(tft, &transaction);
}

// ============================================================
// ENVIA DADOS PARA O DISPLAY
// ============================================================
static void tft_data(const uint8_t *data, size_t length)
{
    spi_transaction_t transaction = {};

    transaction.length = length * 8;
    transaction.tx_buffer = data;

    gpio_set_level(TFT_DC, 1);

    spi_device_transmit(tft, &transaction);
}

// ============================================================
// DEFINE A ÁREA DO DISPLAY ONDE SERÁ DESENHADO
// ============================================================
static void tft_set_window(
    uint8_t x0,
    uint8_t y0,
    uint8_t x1,
    uint8_t y1)
{
    uint8_t columns[] = {
        0,
        x0,
        0,
        x1
    };

    uint8_t rows[] = {
        0,
        y0,
        0,
        y1
    };

    // Column Address Set
    tft_command(0x2A);
    tft_data(columns, sizeof(columns));

    // Row Address Set
    tft_command(0x2B);
    tft_data(rows, sizeof(rows));

    // Memory Write
    tft_command(0x2C);
}
// ============================================================
// PREENCHE UM RETÂNGULO COM UMA COR
// ============================================================
static void tft_fill_rect(
    uint8_t x,
    uint8_t y,
    uint8_t width,
    uint8_t height,
    uint16_t color)
{
    // Prepara o buffer com a cor desejada.
    // O display utiliza RGB565, portanto cada pixel possui 2 bytes.
    for (size_t index = 0; index < TFT_WIDTH; index++)
    {
        line_buffer[index * 2] =
            static_cast<uint8_t>(color >> 8);

        line_buffer[index * 2 + 1] =
            static_cast<uint8_t>(color & 0xFF);
    }
    // Define a área que será preenchida
    tft_set_window(
        x,
        y,
        x + width - 1,
        y + height - 1
    );

    // Envia uma linha por vez
    for (uint8_t row = 0; row < height; row++)
    {
        tft_data(
            line_buffer,
            width * 2
        );
    }
}
// ============================================================
// INICIALIZAÇÃO DO DISPLAY TFT
// ============================================================

static void tft_init(void)
{
    // --------------------------------------------------------
    // CONFIGURAÇÃO DO BARRAMENTO SPI
    // --------------------------------------------------------
    spi_bus_config_t bus_config = {};
    bus_config.mosi_io_num = TFT_MOSI;
    bus_config.miso_io_num = -1;
    bus_config.sclk_io_num = TFT_SCLK;
    bus_config.quadwp_io_num = -1;
    bus_config.quadhd_io_num = -1;
    bus_config.max_transfer_sz =
        TFT_WIDTH * sizeof(uint16_t);

    // --------------------------------------------------------
    // CONFIGURAÇÃO DO DISPOSITIVO SPI
    // --------------------------------------------------------
    spi_device_interface_config_t device_config = {};
    device_config.clock_speed_hz = 20 * 1000 * 1000;
    device_config.mode = 0;
    device_config.spics_io_num = TFT_CS;
    device_config.queue_size = 1;
    // --------------------------------------------------------
    // INICIALIZA O BARRAMENTO SPI
    // --------------------------------------------------------
    spi_bus_initialize(
        SPI2_HOST,
        &bus_config,
        SPI_DMA_CH_AUTO
    );
    // --------------------------------------------------------
    // ADICIONA O DISPLAY AO BARRAMENTO SPI
    // --------------------------------------------------------
    spi_bus_add_device(
        SPI2_HOST,
        &device_config,
        &tft
    );
    // --------------------------------------------------------
    // CONFIGURA OS PINOS DE CONTROLE
    // --------------------------------------------------------
    gpio_set_direction(
        TFT_DC,
        GPIO_MODE_OUTPUT
    );
    gpio_set_direction(
        TFT_RST,
        GPIO_MODE_OUTPUT
    );
    // --------------------------------------------------------
    // RESET DO DISPLAY
    // --------------------------------------------------------
    gpio_set_level(TFT_RST, 0);
    vTaskDelay(
        pdMS_TO_TICKS(100)
    );
    gpio_set_level(TFT_RST, 1);
    vTaskDelay(
        pdMS_TO_TICKS(150)
    );

    // --------------------------------------------------------
    // SOFTWARE RESET
    // --------------------------------------------------------
    tft_command(0x01);

    vTaskDelay(
        pdMS_TO_TICKS(150)
    );

    // --------------------------------------------------------
    // SLEEP OUT
    // --------------------------------------------------------

    tft_command(0x11);

    vTaskDelay(
        pdMS_TO_TICKS(150)
    );


    // --------------------------------------------------------
    // CONFIGURAÇÃO DE COR
    // --------------------------------------------------------

    // 0x05 = RGB565, 16 bits por pixel

    uint8_t color_mode = 0x05;

    tft_command(0x3A);

    tft_data(
        &color_mode,
        1
    );

    // --------------------------------------------------------
    // CONFIGURAÇÃO DE ORIENTAÇÃO
    // --------------------------------------------------------

    uint8_t rotation = 0xC8;

    tft_command(0x36);

    tft_data(
        &rotation,
        1
    );

    // --------------------------------------------------------
    // DISPLAY ON
    // --------------------------------------------------------

    tft_command(0x29);

    vTaskDelay(
        pdMS_TO_TICKS(100)
    );
}

// ============================================================
// FONTE 5x7
// ============================================================

static const uint8_t font_5x7[13][5] =
{
    // 0
    {
        0x3E,
        0x51,
        0x49,
        0x45,
        0x3E
    },

    // 1
    {
        0x00,
        0x42,
        0x7F,
        0x40,
        0x00
    },

    // 2
    {
        0x42,
        0x61,
        0x51,
        0x49,
        0x46
    },

    // 3
    {
        0x21,
        0x41,
        0x45,
        0x4B,
        0x31
    },

    // 4
    {
        0x18,
        0x14,
        0x12,
        0x7F,
        0x10
    },

    // 5
    {
        0x27,
        0x45,
        0x45,
        0x45,
        0x39
    },

    // 6
    {
        0x3C,
        0x4A,
        0x49,
        0x49,
        0x30
    },

    // 7
    {
        0x01,
        0x71,
        0x09,
        0x05,
        0x03
    },

    // 8
    {
        0x36,
        0x49,
        0x49,
        0x49,
        0x36
    },

    // 9
    {
        0x06,
        0x49,
        0x49,
        0x29,
        0x1E
    },

    // espaço / caractere desconhecido
    {
        0x00,
        0x00,
        0x00,
        0x00,
        0x00
    },

    // .
    {
        0x00,
        0x60,
        0x60,
        0x00,
        0x00
    },

    // V
    {
        0x77,
        0x08,
        0x08,
        0x08,
        0x77
    }
};

// ============================================================
// RETORNA O ÍNDICE DO CARACTERE NA FONTE
// ============================================================
static int font_index(char character)
{
    if (character >= '0' && character <= '9')
    {
        return character - '0';
    }
    if (character == '.')
    {
        return 11;
    }
    if (character == 'V')
    {
        return 12;
    }

    return 10;
}
// ============================================================
// ESCREVE TEXTO NO DISPLAY
// ============================================================
static void tft_text(
    const char *text,
    uint8_t x,
    uint8_t y,
    uint8_t scale,
    uint16_t color)
{
    for (; *text != '\0'; text++, x += 6 * scale)
    {
        const uint8_t *glyph =
            font_5x7[font_index(*text)];
        for (uint8_t column = 0; column < 5; column++)
        {
            for (uint8_t row = 0; row < 7; row++)
            {
                if ((glyph[column] >> row) & 1)
                {
                    tft_fill_rect(
                        x + column * scale,
                        y + row * scale,
                        scale,
                        scale,
                        color
                    );
                }
            }
        }
    }
}
// ============================================================
// PROGRAMA PRINCIPAL
// ============================================================
extern "C" void app_main(void)
{
    // Inicializa o display
    tft_init();
    // Limpa toda a tela com preto
    tft_fill_rect(
        0,
        0,
        TFT_WIDTH,
        TFT_HEIGHT,
        0x0000
    );

    float value = 0.0f; // Valor que será exibido
    char value_text[16];
    // Loop principal
    while (true)
    {
        // Limpa a região onde o valor será exibido
        tft_fill_rect(
            8,
            48,
            112,
            56,
            0x0000
        );


        // Converte o valor para texto
        // Exemplo: 12.30V

        snprintf(
            value_text,
            sizeof(value_text),
            "%.2fV",
            value
        );


        // Escreve o valor no display

        tft_text(
            value_text,
            22,
            64,
            4,
            0xFFFF
        );


        // Incrementa o valor

        value += 0.1f;


        // Volta para zero ao chegar em 100 V

        if (value > 99.9f)
        {
            value = 0.0f;
        }


        // Atualiza a cada 1 segundo

        vTaskDelay(
            pdMS_TO_TICKS(1000)
        );
    }
}