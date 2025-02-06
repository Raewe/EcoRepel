#include "stdio.h"
#include "pico/stdlib.h"
#include "hardware/timer.h"
#include "hardware/irq.h"
#include "hardware/adc.h"
#include "hardware/pwm.h"
#include "ssd1306.h"

// Definição dos pinos GPIO
#define LED_RED_PIN 13    // Pino do LED vermelho
#define LED_BLUE_PIN 12   // Pino do LED azul
#define LED_GREEN_PIN 11  // Pino do LED verde
#define BUZZER_PIN_A 10   // Pino do buzzer A
#define BUZZER_PIN_B 21   // Pino do buzzer B
#define OLED_SDA_PIN 14   // Pino SDA do display OLED
#define OLED_SCL_PIN 15   // Pino SCL do display OLED
#define PIR_SENSOR_PIN 5  // Pino do PIR (sensor de movimento)
#define MICROPHONE_PIN 28 // Pino do microfone (simulado por um potenciômetro)

// Definição de constantes
#define SOUND_THRESHOLD 2500    // Limite de som para ativar o alarme
#define ALERT_BLINK_TIME 150    // Tempo de piscar do LED em modo ALERTA
#define IDLE_BLINK_TIME 300     // Tempo de piscar do LED em modo INATIVO
#define ALARM_DURATION_MS 10000 // Duração do alarme em milissegundos
#define I2C_PORT i2c1           // Instância I2C para o display

// Variáveis globais de controle
volatile bool is_message_being_sent = false; // Indica se a mensagem está sendo enviada
volatile bool is_alarm_active = false;       // Indica se o alarme está ativado

void init_pins_config() // Função para inicializar das configurações dos pinos

{
    // Inicializa LED vermelho
    gpio_init(LED_RED_PIN);
    gpio_set_dir(LED_RED_PIN, GPIO_OUT);

    // Inicializa LED verde
    gpio_init(LED_GREEN_PIN);
    gpio_set_dir(LED_GREEN_PIN, GPIO_OUT);

    // Inicializa sensor de movimento PIR
    gpio_init(PIR_SENSOR_PIN);
    gpio_set_dir(PIR_SENSOR_PIN, GPIO_IN);
    gpio_pull_up(PIR_SENSOR_PIN);

    // Inicializa o microfone (simulado por um potenciômetro)
    adc_init();
    adc_gpio_init(MICROPHONE_PIN);
    adc_select_input(2);

    // Inicializa display OLED SSD1306 via I2C
    i2c_init(I2C_PORT, 400 * 1000); // I2C a 400 kHz
    gpio_set_function(OLED_SDA_PIN, GPIO_FUNC_I2C);
    gpio_set_function(OLED_SCL_PIN, GPIO_FUNC_I2C);
    gpio_pull_up(OLED_SDA_PIN);
    gpio_pull_up(OLED_SCL_PIN);
    ssd1306_init(I2C_PORT);
    ssd1306_clear();
}
void display_text(uint pos_x, uint pos_y, const char *message)
{
    ssd1306_draw_string(pos_x, pos_y, message, true);
    ssd1306_update(I2C_PORT);
}

void display_clear()
{
    ssd1306_clear();
    ssd1306_update(I2C_PORT);
}

void blink_leds_off(uint pin)
{
    uint slice = pwm_gpio_to_slice_num(pin);
    pwm_set_enabled(slice, false);
    gpio_set_function(pin, GPIO_FUNC_SIO); // Redefine o GPIO como saída
    gpio_put(pin, 0);
}

void blink_leds_on(uint pin, uint frequency, uint brightnessInPercentage) // faz o led piscar
{
    gpio_set_function(pin, GPIO_FUNC_PWM);
    uint16_t wrap = (uint16_t)(125000000 / frequency) - 1; // Fórmula para calcular o Wrap
    uint slice = pwm_gpio_to_slice_num(pin);
    uint duty_cycle = (wrap * brightnessInPercentage) / 100;
    pwm_set_wrap(slice, wrap);
    pwm_set_gpio_level(pin, duty_cycle);
    pwm_set_clkdiv(slice, 1.0);
    pwm_set_enabled(slice, true);
    sleep_ms(is_alarm_active ? ALERT_BLINK_TIME : IDLE_BLINK_TIME); // Determina que tempo o led deve deve durar
    blink_leds_off(pin);
}

void play_tone(uint pin, float frequency, uint duration_ms) // Faz o som no buzzer usando pwm
{
    gpio_set_function(pin, GPIO_FUNC_PWM);
    uint slice_num = pwm_gpio_to_slice_num(pin);
    uint16_t wrap = (uint16_t)(125000000 / frequency) - 1; // Fórmula para calcular o Wrap
    pwm_set_wrap(slice_num, wrap);
    pwm_set_chan_level(slice_num, pwm_gpio_to_channel(pin), (wrap + 1) / 2);
    pwm_set_enabled(slice_num, true);
    sleep_ms(duration_ms);
    pwm_set_enabled(slice_num, false); // Desliga o buzzer
}
void play_alarm() // Ativa o som e o led do alarme
{
    blink_leds_off(LED_GREEN_PIN);
    for (int i = 0; i < 3; i++)
    {
        blink_leds_on(LED_RED_PIN, 10000, 100);
        play_tone(BUZZER_PIN_A, 800.0, 200);
        play_tone(BUZZER_PIN_B, 800.0, 200);
        sleep_ms(100);

        blink_leds_on(LED_RED_PIN, 10000, 100);
        play_tone(BUZZER_PIN_A, 1600.0, 200);
        play_tone(BUZZER_PIN_B, 1600.0, 200);
        sleep_ms(100);
    }
    sleep_ms(300);
    display_clear();
}

// Callback que é chamado após o envio da mensagem
bool on_message_sent_callback(alarm_id_t id, void *user_data)
{
    is_message_being_sent = false;
    display_clear();
    display_text(8, 16, "Mensagem Enviada!");
    printf("Mensagem de alerta enviada com sucesso!");
    return false;
}

void send_message_to_base() // Função que simula o envio de uma mensagem para um dispositivo remoto
{
    uint32_t current_time_ms = to_ms_since_boot(get_absolute_time());
    uint32_t seconds = current_time_ms / 1000;
    uint32_t minutes = seconds / 60;
    uint32_t hours = minutes / 60;
    uint32_t days = hours / 24;
    uint32_t hour_of_day = hours % 24;
    uint32_t minute_of_hour = minutes % 60;
    uint32_t second_of_minute = seconds % 60;
    is_message_being_sent = true;
    printf("Tentando enviar mensagem para o dispositivo remoto: Dia %d, Hora %02d:%02d:%02d\n", days, hour_of_day, minute_of_hour, second_of_minute);
    add_alarm_in_ms(6000, on_message_sent_callback, NULL, false);
}

bool on_alarm_timeout_callback() // Desliga o alarme
{
    is_alarm_active = false;
    return false;
}

void trigger_alarm() // Dispara o alarme
{
    is_alarm_active = true;
    add_alarm_in_ms(ALARM_DURATION_MS, on_alarm_timeout_callback, NULL, false);
}

void gpio_irq_handler(uint gpio, uint32_t events) // Função callback para as interrupções dos GPIOs
{
    if (!is_message_being_sent && !is_alarm_active)
    {
        if (gpio == PIR_SENSOR_PIN)
        {
            printf("Movimento Detectado!!!\n");
            trigger_alarm();
            send_message_to_base();
            display_clear();
        }
    }
}

bool adc_check_callback(struct repeating_timer *t) // Função callback do temporizador ADC
{
    if (!is_message_being_sent && !is_alarm_active)
    {
        uint16_t adc_value = adc_read();

        printf("ADC Value: %u\n", adc_value);

        if (adc_value > SOUND_THRESHOLD) // Verifica se o valor ultrapassa o limite de som permitido
        {
            printf("Som alto detectado!\n");
            trigger_alarm();
            send_message_to_base();
            display_clear();
        }
        return true;
    }
}
int main()
{
    // Inicialização das principais configurações do sistema
    stdio_init_all();
    init_pins_config();

    // Configura as interrupções dos GPIOs
    gpio_set_irq_enabled_with_callback(PIR_SENSOR_PIN, GPIO_IRQ_EDGE_RISE, true, gpio_irq_handler);

    // Checa se houve alguma mudança significativa no adc a cada 100ms
    struct repeating_timer timer;
    add_repeating_timer_ms(100, adc_check_callback, NULL, &timer);

    while (true)
    {
        if (is_alarm_active || is_message_being_sent)
        {
            display_text(8, 16, "Sistema em Alerta!");
            play_alarm();
        }
        else
        {
            display_text(8, 0, "Sistema Funcionando!");
            blink_leds_off(LED_RED_PIN);
            blink_leds_on(LED_GREEN_PIN, 10000, 50);
        }
        sleep_ms(200);
    }
    return 0;
}
