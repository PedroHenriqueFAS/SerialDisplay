#include <stdio.h>
#include <stdlib.h>
#include "pico/stdlib.h"
#include "hardware/pio.h"
#include "hardware/clocks.h"
#include "matriz_leds.h"

// Arquivo .pio para controle da matriz
#include "pio_matrix.pio.h"

// Pino que realizará a comunicação do microcontrolador com a matriz
// Como o pino do simulador eh diferente do pino da placa (7), deve-se alterar  
#define OUT_PIN 27

// Gera o binário que controla a cor de cada célula do LED
//rotina para definição da intensidade de cores do led
uint32_t gerar_binario_cor(double red, double green, double blue)
{
  unsigned char RED, GREEN, BLUE;
  RED = red * 255.0;
  GREEN = green * 255.0;
  BLUE = blue * 255.0;
  return (GREEN << 24) | (RED << 16) | (BLUE << 8);
}

uint configurar_matriz(PIO pio, uint8_t pin){
    bool ok;

    // Define o clock para 128 MHz, facilitando a divisão pelo clock
    ok = set_sys_clock_khz(128000, false);

    // Inicializa todos os códigos stdio padrão que estão ligados ao binário.
    stdio_init_all();

    printf("iniciando a transmissão PIO");
    if (ok) printf("clock set to %ld\n", clock_get_hz(clk_sys));

    //configurações da PIO
    uint offset = pio_add_program(pio, &pio_matrix_program);
    uint sm = pio_claim_unused_sm(pio, true);
    pio_matrix_program_init(pio, sm, offset, pin);

    return sm;
}

void imprimir_desenho(Matriz_leds_config configuracao, PIO pio, uint sm){
    for (int contadorLinha = 4; contadorLinha >= 0; contadorLinha--){
        if(contadorLinha % 2){
            for (int contadorColuna = 0; contadorColuna < 5; contadorColuna ++){
                uint32_t valor_cor_binario = gerar_binario_cor(
                    configuracao[contadorLinha][contadorColuna].red,
                    configuracao[contadorLinha][contadorColuna].green,
                    configuracao[contadorLinha][contadorColuna].blue
                );

                pio_sm_put_blocking(pio, sm, valor_cor_binario);
            }
        }else{
            for (int contadorColuna = 4; contadorColuna >= 0; contadorColuna --){
                uint32_t valor_cor_binario = gerar_binario_cor(
                    configuracao[contadorLinha][contadorColuna].red,
                    configuracao[contadorLinha][contadorColuna].green,
                    configuracao[contadorLinha][contadorColuna].blue
                );

                pio_sm_put_blocking(pio, sm, valor_cor_binario);
            }
        }
    }
}

RGB_cod obter_cor_por_parametro_RGB(int red, int green, int blue){
    RGB_cod cor_customizada = {red/255.0,green/255.0,blue/255.0};

    return cor_customizada;
}

void hex_to_rgb(uint32_t hex_animation[][25], rgb_led rgb_data[][25], uint8_t frames) {
    for(int frame = 0; frame < frames; frame++) {
        for (int j = 0; j < 25; j++) {
            uint32_t hex = hex_animation[frame][j];
            rgb_data[frame][j].r = (hex >> 16) & 0xFF;
            rgb_data[frame][j].g = (hex >> 8) & 0xFF;
            rgb_data[frame][j].b = hex & 0xFF;
        }
    }   
}

void ajustar_brilho(rgb_led matriz[][25], float brightness, uint8_t frames) {
    for (int i = 0; i < frames; i++) {
        for (int j = 0; j < 5 * 5; j++) {
            matriz[i][j].r = (int)(matriz[i][j].r * brightness);
            matriz[i][j].g = (int)(matriz[i][j].g * brightness);
            matriz[i][j].b = (int)(matriz[i][j].b * brightness);
        }
    }
}

void converter_RGB_para_matriz_leds(rgb_led matriz[5][5], Matriz_leds_config matriz_leds) {
    for (int linha = 0; linha < 5; linha++) {
        for (int coluna = 0; coluna < 5; coluna++) {
            matriz_leds[linha][coluna].red = matriz[linha][coluna].r / 255.0;
            matriz_leds[linha][coluna].green = matriz[linha][coluna].g / 255.0;
            matriz_leds[linha][coluna].blue = matriz[linha][coluna].b / 255.0;
        }
    }
}

void enviar_animacao(rgb_led matriz[][25], PIO pio, uint sm, uint8_t frames) {
    Matriz_leds_config matriz_leds;
    rgb_led raw_matrix[5][5];

    for(uint8_t frame = 0; frame < frames; frame++){
        for(uint8_t i = 0; i < 25; i++){
            uint8_t row = i / 5;
            uint8_t col = i % 5;
            raw_matrix[row][col] = matriz[frame][i];
        }
        converter_RGB_para_matriz_leds(raw_matrix, matriz_leds);
        imprimir_desenho(matriz_leds, pio, sm);
        sleep_ms(300);
    }
}

void limpar_matriz(PIO pio, uint sm) {
    Matriz_leds_config matriz;
    for(int i = 0; i < 5; i++) {
        for (int j = 0; j < 5; j++) {
            matriz[i][j].red = 0.0;
            matriz[i][j].green = 0.0;
            matriz[i][j].blue = 0.0;
        }
    }
    imprimir_desenho(matriz,pio,sm);
}