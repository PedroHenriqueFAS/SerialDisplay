#include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"
#include "hardware/i2c.h"
#include "hardware/uart.h"
#include "hardware/irq.h"
#include "hardware/pio.h"
#include "hardware/timer.h"
#include "lib/ssd1306_i2c.h"
#include "lib/matriz_leds.h"
#include <stdint.h>

#define BUTTON_H
#define LED_H
#define I2C_SDA 14
#define I2C_SDL 15
#define BTN_A 5
#define BTN_B 6
#define LED_G 11
#define LED_B 12
#define MATRIX_PIN 7

extern Matriz_leds_config* numeros[];

// Gerencia o debounce dos botões
static uint32_t last_time = 0;

// Variáveis globais para controle dos LEDs
static volatile uint8_t number;
static volatile bool led_g_state = false;
static volatile bool led_b_state = false;

// Variáveis para configurar a matriz de LEDs
PIO pio;
uint sm;

// Buffer para manipulação do display
uint8_t ssd[ssd1306_buffer_length];

// Estrutura para definir a área de renderização no display
struct render_area frame_area = {
    start_column : 0,
    end_column : ssd1306_width - 1,
    start_page : 0,
    end_page : ssd1306_n_pages - 1
};

// Protótipos das funções
static void gpio_irq_handler(uint gpio, uint32_t events);
static bool debounce_time(uint32_t *last_time);
void button_init(uint pin);
void led_init(uint8_t pin);
void blink(uint8_t pin);

// Declaração de funções externas do display
extern void calculate_render_area_buffer_length(struct render_area *area);
extern void ssd1306_send_command(uint8_t cmd);
extern void ssd1306_send_command_list(uint8_t *ssd, int number);
extern void ssd1306_send_buffer(uint8_t ssd[], int buffer_length);
extern void ssd1306_init();
extern void ssd1306_scroll(bool set);
extern void render_on_display(uint8_t *ssd, struct render_area *area);
extern void ssd1306_set_pixel(uint8_t *ssd, int x, int y, bool set);
extern void ssd1306_draw_line(uint8_t *ssd, int x_0, int y_0, int x_1, int y_1, bool set);
extern void ssd1306_draw_char(uint8_t *ssd, int16_t x, int16_t y, uint8_t character);
extern void ssd1306_draw_string(uint8_t *ssd, int16_t x, int16_t y, char *string);
extern void ssd1306_command(ssd1306_t *ssd, uint8_t command);
extern void ssd1306_config(ssd1306_t *ssd);
extern void ssd1306_init_bm(ssd1306_t *ssd, uint8_t width, uint8_t height, bool external_vcc, uint8_t address, i2c_inst_t *i2c);
extern void ssd1306_send_data(ssd1306_t *ssd);
extern void ssd1306_draw_bitmap(ssd1306_t *ssd, const uint8_t *bitmap);

int main() {
    char input;

    stdio_init_all();

    // Inicializa os botões e LEDs
    button_init(BTN_A);
    button_init(BTN_B);
    led_init(LED_G);
    led_init(LED_B);

    // Configuração da matriz de LEDs
    pio = pio0;
    sm = configurar_matriz(pio, MATRIX_PIN);
   
    // Inicialização do display I2C
    i2c_init(i2c1, ssd1306_i2c_clock * 1000);
    gpio_set_function(I2C_SDA, GPIO_FUNC_I2C);
    gpio_set_function(I2C_SDL, GPIO_FUNC_I2C);
    gpio_pull_up(I2C_SDA);
    gpio_pull_up(I2C_SDL);

    ssd1306_init();

    // Configuração da área de renderização
    calculate_render_area_buffer_length(&frame_area);

    // Zera o buffer do display
    memset(ssd, 0, ssd1306_buffer_length);
    render_on_display(ssd, &frame_area);

    // Desliga a matriz de LEDs
    imprimir_desenho(*numeros[10], pio, sm);

    // Ativa as interrupções para os botões
    gpio_set_irq_enabled_with_callback(BTN_A, GPIO_IRQ_EDGE_FALL, true, gpio_irq_handler);
    gpio_set_irq_enabled_with_callback(BTN_B, GPIO_IRQ_EDGE_FALL, true, gpio_irq_handler);

    while (true) {
        scanf("%c", &input);
        printf("%c", input);

        // Limpa o display
        memset(ssd, 0, ssd1306_buffer_length);
        
        // Verifica se o input é um número e exibe na matriz de LEDs
        if (input >= '0' && input <= '9'){
            imprimir_desenho(*numeros[input - '0'], pio, sm);
        } else {
            imprimir_desenho(*numeros[10], pio, sm);
        }

        // Renderiza o caractere digitado no display
        ssd1306_draw_char(ssd, 10, 10, input);
        render_on_display(ssd, &frame_area);    
        sleep_ms(1000);
    }
}

// Inicializa um botão
void button_init(uint pin) {
    gpio_init(pin);
    gpio_set_dir(pin, GPIO_IN);
    gpio_pull_up(pin);
}

// Inicializa um LED
void led_init(uint8_t pin){
    gpio_init(pin);
    gpio_set_dir(pin, GPIO_OUT);
}

// Função para piscar um LED
void blink(uint8_t pin) {
    gpio_put(pin, true);
    sleep_ms(100); // Acende o LED por 100 ms
    gpio_put(pin, false);
    sleep_ms(100); // Apaga o LED por 100 ms
}

// Manipula interrupções dos botões
static void gpio_irq_handler(uint gpio, uint32_t events) {
    uint32_t current_time = to_us_since_boot(get_absolute_time());

    // Vetor para armazenar o texto a ser exibido no display
    char *text[2] ={ "", "" };

    // Aplica o debounce
    if(debounce_time(&last_time)) {
        if(gpio == BTN_A) {  
            led_g_state = !led_g_state;
            gpio_put(LED_G, led_g_state);

            text[0] = "  LED Verde  ";
            text[1] = led_g_state ? "   ligado  " : "  desligado  ";
            printf("LED verde %s\n", led_g_state ? "ligado" : "desligado");
        } else if (gpio == BTN_B) {
            led_b_state = !led_b_state;
            gpio_put(LED_B, led_b_state);

            text[0] = "  LED Azul  ";
            text[1] = led_b_state ? "   ligado  " : "  desligado  ";
            printf("LED azul %s\n", led_b_state ? "ligado" : "desligado");
        }
    }

    // Renderiza o texto no display
    int y = 0;
    for(uint8_t i = 0; i < count_of(text); i++) {
        ssd1306_draw_string(ssd, 5, y, text[i]);
        y += 8;
    }
    render_on_display(ssd, &frame_area);
}

// Implementa a lógica de debounce
static bool debounce_time(uint32_t *last_time) {
    uint32_t current_time = to_us_since_boot(get_absolute_time());
    if(current_time - *last_time > 200000) {
        *last_time = current_time;
        return true;
    }
    return false;
}
