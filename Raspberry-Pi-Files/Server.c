#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <wiringPi.h>

#define PORT 12345
#define LDR_PIN 7       // Physical Pin 7
#define TRIG_PIN 0      // Physical Pin 11 (GPIO 17)
#define ECHO_PIN 2      // Physical Pin 13 (GPIO 27)
#define DHT_PIN 3       // Physical Pin 15 (GPIO 22)
#define DHT_TYPE DHT11  // DHT11 Sensor

int readLDRValue()
{
    pinMode(LDR_PIN, INPUT);
    int pulse_count = 0;
    int start_time = millis();

    while ((millis() - start_time) < 1000)  // Count pulses for 1 second
    {
        if (digitalRead(LDR_PIN) == LOW)
        {
            pulse_count++;
        }
    }

    return pulse_count;  // Use pulse count as an estimate of light intensity
}

float readUltrasonicDistance()
{
    pinMode(TRIG_PIN, OUTPUT);
    pinMode(ECHO_PIN, INPUT);
    digitalWrite(TRIG_PIN, LOW);
    delay(30);

    digitalWrite(TRIG_PIN, HIGH);
    delayMicroseconds(20);
    digitalWrite(TRIG_PIN, LOW);

    while (digitalRead(ECHO_PIN) == LOW)
    {
        continue;
    }

    long startTime = micros();
    while (digitalRead(ECHO_PIN) == HIGH)
    {
        continue;
    }

    long travelTime = micros() - startTime;
    float distance = travelTime / 58.0;  // Speed of sound in air is approximately 340 m/s or 29 microseconds per centimeter
    return distance;
}

void readDHT11Data(float *humidity, float *temperature)
{
    int data[5] = {0, 0, 0, 0, 0};

    pinMode(DHT_PIN, OUTPUT);
    digitalWrite(DHT_PIN, LOW);
    delay(18);
    digitalWrite(DHT_PIN, HIGH);
    delayMicroseconds(40);
    pinMode(DHT_PIN, INPUT);

    // Wait for sensor response
    while (digitalRead(DHT_PIN) == HIGH)
        delayMicroseconds(1);

    // Sensor should pull low for 80us
    while (digitalRead(DHT_PIN) == LOW)
        delayMicroseconds(1);

    // Sensor should pull high for 80us
    while (digitalRead(DHT_PIN) == HIGH)
        delayMicroseconds(1);

    // Read 40 bits of data
    for (int i = 0; i < 40; i++)
    {
        // Data bit starts with a low state for 50us
        while (digitalRead(DHT_PIN) == LOW)
            delayMicroseconds(1);

        // Measure the length of the high state to determine if it's a 0 or 1
        int counter = 0;
        while (digitalRead(DHT_PIN) == HIGH)
        {
            delayMicroseconds(1);
            counter++;
        }

        // Store the bit by shifting data array
        data[i / 8] <<= 1;
        if (counter > 30)
            data[i / 8] |= 1;
    }

    // Verify checksum
    if (data[4] == ((data[0] + data[1] + data[2] + data[3]) & 0xFF))
    {
        *humidity = (float)data[0];
        *temperature = (float)data[2];
    }
    else
    {
        *humidity = -1.0;    // Error value for humidity
        *temperature = -1.0; // Error value for temperature
    }
}

int main()
{
    if (wiringPiSetup() == -1)
    {
        printf("Failed to initialize WiringPi library.\n");
        return 1;
    }

    int server_fd, new_socket, valread;
    struct sockaddr_in address;
    int addrlen = sizeof(address);
    char buffer[1024] = {0};
    char response[1024] = {0};

    // Create socket file descriptor
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0)
    {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);

    // Bind the socket to given IP and port
    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0)
    {
        perror("bind failed");
        exit(EXIT_FAILURE);
    }

    // Listen for incoming connections
    if (listen(server_fd, 3) < 0)
    {
        perror("listen failed");
        exit(EXIT_FAILURE);
    }

    printf("Server listening on port %d\n", PORT);

    // Accept incoming connection
 if ((new_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t *)&addrlen)) < 0)
    {
        perror("accept failed");
        exit(EXIT_FAILURE);
    }

    printf("Connected\n");

    while (1)
    {
        // Read LDR sensor data and calculate light intensity
        int ldrValue = readLDRValue();

        // Read Ultrasonic sensor data
        float ultrasonicDistance = readUltrasonicDistance();

        // Read DHT11 sensor data
        float humidity, temperature;
        readDHT11Data(&humidity, &temperature);

        // Send the sensor data to the client

        if (humidity != -1.0 && temperature != -1.0 && ldrValue!= -1.0 && ultrasonicDistance>0)
        {
            sprintf(response, "\nTemperature: %.1f°C, Humidity: %.1f%% Ultrasonic Distance: %.2f cm light Intensity: %d ", temperature, humidity,ultrasonicDistance,ldrValue);
        }
        else
        {
            sprintf(response, "Failed to retrieve data from sensor.\n");
        }
        send(new_socket, response, strlen(response), 0);

        delay(1000);
    }

    close(new_socket);
    close(server_fd);

    return 0;
}
