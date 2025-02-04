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
#define BUZZER_PIN_A 21   // Pino do buzzer 2
#define BUZZER_PIN_B 10   // Pino do buzzer
#define OLED_SDA_PIN 14   // Pino SDA do display OLED
#define OLED_SCL_PIN 15   // Pino SCL do display OLED
#define PIR_SENSOR_PIN 5  // Pino do PIR (sensor de movimento)
#define MICROPHONE_PIN 28 // Pino do microfone (simulado por um potenciômetro)

// Definição de constantes
#define SOUND_THRESHOLD 2000    // Limite de som para ativar uma ação
#define ALERT_BLINK_TIME 300    // Tempo de piscar do LED em modo ALERTA
#define IDLE_BLINK_TIME 400     // Tempo de piscar do LED em modo INATIVO
#define BUZZER_BEEP_DELAY 100   // Tempo de delay do beep do buzzer
#define ALARM_DURATION_MS 10000 // Duração do alarme em milissegundos
#define I2C_PORT i2c1           // Instância I2C para o display

// Variáveis globais de controle
volatile bool isMessageBeingSent = false; // Indica se a mensagem está sendo enviada
volatile bool isAlarmActive = false;      // Indica se o alarme está ativado

// Função para inicializar os pinos GPIO
void init_gpio_pins()
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
    adc_init(); // Inicializa o potenciômetro
    adc_gpio_init(MICROPHONE_PIN);
    adc_select_input(0); // Seleciona o canal 0 (pino GP28)

    // Inicializa display OLED SSD1306 via I2C
    i2c_init(I2C_PORT, 400 * 1000); // I2C a 400 kHz
    gpio_set_function(OLED_SDA_PIN, GPIO_FUNC_I2C);
    gpio_set_function(OLED_SCL_PIN, GPIO_FUNC_I2C);
    gpio_pull_up(OLED_SDA_PIN);
    gpio_pull_up(OLED_SCL_PIN);
    ssd1306_init(I2C_PORT);
    ssd1306_clear();
}

void display_text(uint posX, uint posY, const char *message)
{
    ssd1306_draw_string(posX, posY, message, true);
    ssd1306_update(I2C_PORT);
}
void display_clear()
{
    ssd1306_clear();
    ssd1306_update(I2C_PORT);
}

// Callback que é chamado após o envio da mensagem
int64_t on_message_sent_callback(alarm_id_t id, void *user_data)
{
    printf("Mensagem Enviada com Sucesso!!!\n");
    display_text(8, 32, "Mensagem Enviada!!!");
    isMessageBeingSent = false;
    return 0;
}

// Função que simula o envio de uma mensagem para um dispositivo remoto
void send_message_to_base()
{
    uint32_t current_time_ms = to_ms_since_boot(get_absolute_time());
    uint32_t seconds = current_time_ms / 1000;
    uint32_t minutes = seconds / 60;
    uint32_t hours = minutes / 60;
    uint32_t days = hours / 24;

    uint32_t hour_of_day = hours % 24;
    uint32_t minute_of_hour = minutes % 60;
    uint32_t second_of_minute = seconds % 60;

    isMessageBeingSent = true;
    printf("Tentando enviar mensagem para o dispositivo remoto: Dia %d, Hora %02d:%02d:%02d\n", days, hour_of_day, minute_of_hour, second_of_minute);
    add_alarm_in_ms(6000, on_message_sent_callback, NULL, false);
}

// Callback chamado quando o tempo do alarme se esgota
int64_t on_alarm_timeout_callback()
{
    isAlarmActive = false;
    printf("alarme acabou!\n");
    return 0;
}

// Função para disparar o alarme
void trigger_alarm()
{
    isAlarmActive = true;
    add_alarm_in_ms(ALARM_DURATION_MS, on_alarm_timeout_callback, NULL, false);
    printf("Alarme Disparado!\n");
    display_text(8, 32, "O Alarme disparou !");
}

// Função de callback para as interrupções dos GPIOs
void gpio_irq_handler(uint gpio, uint32_t events)
{
    if (!isMessageBeingSent && !isAlarmActive)
    {
        if (gpio == PIR_SENSOR_PIN)
        {
            printf("--------------------------------------------\n");
            printf("Movimento Detectado!!!\n");
            trigger_alarm();
            send_message_to_base();
        }
    }
}

void blink_leds_off(uint pin)
{
    uint slice = pwm_gpio_to_slice_num(pin);
    pwm_set_enabled(slice, false);
    gpio_set_function(pin, GPIO_FUNC_SIO); // Redefine o GPIO como saída
    gpio_put(pin, 0);
}

void blink_leds_on(uint pin, uint hertz, uint brightnessInPercentage)
{
    gpio_set_function(pin, GPIO_FUNC_PWM);
    uint wrap = (125000000 / hertz) - 1; // Fórmula para calcular o Wrap
    uint slice = pwm_gpio_to_slice_num(pin);
    uint dutycycle = (wrap * brightnessInPercentage) / 100;
    pwm_set_wrap(slice, wrap);
    pwm_set_gpio_level(pin, dutycycle);
    pwm_set_clkdiv(slice, 1.0);
    pwm_set_enabled(slice, true);
    sleep_ms(isAlarmActive ? ALERT_BLINK_TIME : IDLE_BLINK_TIME);
    blink_leds_off(pin); // Desliga o LED após o tempo
}

void play_tone(uint buzzer, float frequency, uint duration_ms)
{
    // Habilita a função PWM no pino do buzzer
    gpio_set_function(buzzer, GPIO_FUNC_PWM);

    uint slice_num = pwm_gpio_to_slice_num(buzzer);

    // Configura o "wrap" para a frequência
    pwm_set_wrap(slice_num, 125000000 / frequency - 1);

    pwm_set_chan_level(slice_num, pwm_gpio_to_channel(buzzer), (125000000 / frequency) * 0.5); // Aumenta o duty cycle para 80%

    // Habilita o PWM para gerar o som
    pwm_set_enabled(slice_num, true);

    // Espera pela duração do som
    sleep_ms(duration_ms);

    // Desliga o buzzer
    pwm_set_enabled(slice_num, false);        // Desativa o PWM
    gpio_set_function(buzzer, GPIO_FUNC_SIO); // Redefine o GPIO como saída padrão
}
void play_alarm()
{
    // Primeira sequência - crescendo de tom
    play_tone(BUZZER_PIN_A, 1500.0, 180);
    play_tone(BUZZER_PIN_B, 1500.0, 180);
    sleep_ms(50);

    play_tone(BUZZER_PIN_A, 2000.0, 180);
    play_tone(BUZZER_PIN_B, 2000.0, 180);
    sleep_ms(50);

    play_tone(BUZZER_PIN_A, 2500.0, 180);
    play_tone(BUZZER_PIN_B, 2500.0, 180);
    sleep_ms(100);

    // Segunda sequência - variação com efeito pulsante
    play_tone(BUZZER_PIN_A, 3000.0, 120);
    play_tone(BUZZER_PIN_B, 3000.0, 120);
    sleep_ms(30);

    play_tone(BUZZER_PIN_A, 2800.0, 120);
    play_tone(BUZZER_PIN_B, 2800.0, 120);
    sleep_ms(30);

    play_tone(BUZZER_PIN_A, 3500.0, 200);
    play_tone(BUZZER_PIN_B, 3500.0, 200);
    sleep_ms(200);

    // Terceira sequência - descendo para dar impacto
    play_tone(BUZZER_PIN_A, 2500.0, 180);
    play_tone(BUZZER_PIN_B, 2500.0, 180);
    sleep_ms(50);

    play_tone(BUZZER_PIN_A, 2000.0, 180);
    play_tone(BUZZER_PIN_B, 2000.0, 180);
    sleep_ms(50);

    play_tone(BUZZER_PIN_A, 1500.0, 250);
    play_tone(BUZZER_PIN_B, 1500.0, 250);
    sleep_ms(300);
}

// Função que verifica o valor do ADC (potenciômetro)
bool adc_check_callback(struct repeating_timer *t)
{
    if (!isMessageBeingSent && !isAlarmActive)
    {

        uint16_t adc_value = adc_read();
        printf("ADC Value: %u\n", adc_value);

        // Verifica se o valor ultrapassa o limite de som permitido
        if (adc_value > SOUND_THRESHOLD)
        {
            printf("Som alto detectado!\n");
            trigger_alarm();
            send_message_to_base();
        }
        return true; // Retorna true para manter o temporizador ativo
    }
}
// Função principal do programa
int main()
{
    stdio_init_all(); // Inicializa a comunicação serial
    init_gpio_pins(); // Inicializa os pinos GPIO // TODO: Mudar o nome ? e colocar a incialização aqui ?

    // TODO: FAzer o Checkup dos equipamentos

    // Configura as interrupções dos GPIOs
    gpio_set_irq_enabled_with_callback(PIR_SENSOR_PIN, GPIO_IRQ_EDGE_RISE, true, gpio_irq_handler);

    // Configura o temporizador para chamar o ADC a cada 100ms

    struct repeating_timer timer;
    add_repeating_timer_ms(200, adc_check_callback, NULL, &timer); // Aumente o intervalo para 500ms

    printf("Sistema iniciado...\n");
    display_text(8, 0, "Sistema iniciando...");

    while (true)
    {
        if (isAlarmActive || isMessageBeingSent) // Verifica se o alarme está tocando ou se a mensagem ainda está sendo enviada

        {
            display_text(8, 16, "Sistema em Alerta! ");
            blink_leds_off(LED_GREEN_PIN); // TODO: Quando o som está tocando o Led não pisca
            blink_leds_on(LED_RED_PIN, 1000, 30);
            play_alarm();
        }
        else
        {
            display_text(8, 0, "Sistema Funcionando!");
            blink_leds_off(LED_RED_PIN);
            blink_leds_on(LED_GREEN_PIN, 10000, 50);
        }

        sleep_ms(200); // Faz uma pequena pausa para evitar sobrecarga no loop
    }
    return 0;
}
