#include "rig_control.h"
#include <WiFi.h>

// Rig server IP and port (update these as needed)
static const char* rig_ip = "192.168.1.119";
static const uint16_t rig_port = 4532;

// Initialize socket connection to the rig
int init_rig_socket() {
    if (!rigClient.connect(rig_ip, rig_port)) {
        Serial.println("Failed to connect to rig");
        return -1;
    }
    Serial.println("Connected to rig");
    return 0;
}

// Close socket connection to the rig
void close_rig_socket() {
    if (rigClient.connected()) {
        rigClient.stop();
        Serial.println("Disconnected from rig");
    }
}

// Helper function to send command and receive response
int send_command(const char *command, char *response = nullptr, size_t response_size = 0, int lines_expected = 1) {
    if (!rigClient.connected()) {
        Serial.println("Reconnecting to rig...");
        if (!rigClient.connect(rig_ip, rig_port)) {
            Serial.println("Failed to reconnect to rig");
            return -1;
        }
    }

    // Send the command
    rigClient.print(command);
    Serial.println(command);

    if (lines_expected <= 0) return 0;  // Skip response handling if no response is expected

    // Initialize timeout and response line counters
    unsigned long timeout = millis();
    int index = 0, lines_received = 0;

    while (millis() - timeout < 1000) {  // 1-second timeout
        while (rigClient.available()) {
            char c = rigClient.read();
            if (index < response_size - 1) {
                response[index++] = c;
            }
            if (c == '\n') {
                response[index - 1] = '\0';  // Terminate this line
                lines_received++;
                if (lines_received >= lines_expected) return 0;  // Exit if all lines are received

                // Move to the next line in response buffer
                response += index;
                response_size -= index;
                index = 0;
            }
        }
    }

    Serial.println("Response timeout");
    return -1;
}



// Helper functions for string-to-enum conversion
RigMode str_to_mode(const char *mode_str) {
    if (strcmp(mode_str, "USB") == 0) return USB;
    if (strcmp(mode_str, "LSB") == 0) return LSB;
    return MODE_UNSUPPORTED;
}

RigVFO str_to_vfo(const char *vfo_str) {
    if (strcmp(vfo_str, "VFOA") == 0) return VFOA;
    if (strcmp(vfo_str, "VFOB") == 0) return VFOB;
    return VFO_UNSUPPORTED;
}

// Set Frequency
void set_freq(float frequency) {
    char command[50];
    snprintf(command, sizeof(command), "F %.2f\n", frequency);
    send_command(command, nullptr, 0, false);  // Skip waiting for response
}

// Get Frequency
int get_freq(float *frequency) {
    char command[] = "f\n";
    char response[50];
    
    // Send command and check if a response was received
    if (send_command(command, response, sizeof(response)) == 0) {
        if (response[0] == '\0' || response[0] == '0') {
            Serial.println("Invalid response: Empty or zero.");
            return -1;
        }

        *frequency = atof(response);
        
        if (*frequency == 0.0) {
            Serial.println("Invalid frequency value: Conversion failed or zero frequency.");
            return -1;
        }
        
        return (int)*frequency;
    }

    Serial.println("Failed to receive a response from rig.");
    return -1;
}

// Set Mode and Passband
void set_mode(RigMode mode, int32_t passband) {
    const char *mode_str = "USB";  // Default to USB if unsupported mode
    switch (mode) {
        case USB: mode_str = "USB"; break;
        case LSB: mode_str = "LSB"; break;
        case CW: mode_str = "CW"; break;
        case CWR: mode_str = "CWR"; break;
        case DIGI: mode_str = "DIGI"; break;
        case AM: mode_str = "AM"; break;
        default: mode_str = "MODE_UNSUPPORTED"; break;
    }
    char command[50];
    snprintf(command, sizeof(command), "M %s %d\n", mode_str, passband);
    send_command(command, nullptr, 0, false);  // Skip waiting for response
    delay(200);
}

// Get Mode and Passband
int get_mode(RigMode *mode, int32_t *passband = nullptr) {
    char command[] = "m\n";
    char response[100];  // Buffer large enough for two lines

    // Request two lines from `send_command`
    if (send_command(command, response, sizeof(response), 2) == 0) {
        // Parse the mode (first line)
        char *mode_line = strtok(response, "\n");
        if (mode_line != nullptr) {
            *mode = str_to_mode(mode_line);
        }

        // Parse the passband (second line), if needed
        if (passband) {
            char *passband_line = strtok(nullptr, "\n");
            if (passband_line != nullptr) {
                *passband = atoi(passband_line);
            }
        }

        return (int)*mode;
    }

    return -1;
}




// Set VFO
void set_vfo(RigVFO vfo) {
    const char *vfo_str = "VFOA";  // Default to VFOA if unsupported VFO
    switch (vfo) {
        case VFOA: vfo_str = "VFOA"; break;
        case VFOB: vfo_str = "VFOB"; break;
        default: vfo_str = "VFO_UNSUPPORTED"; break;
    }
    char command[50];
    snprintf(command, sizeof(command), "V %s\n", vfo_str);
    send_command(command, nullptr, 0, false);  // Skip waiting for response
}

// Get VFO
int get_vfo(RigVFO *vfo) {
    char command[] = "v\n";
    char response[50];
    if (send_command(command, response, sizeof(response)) == 0) {
        *vfo = str_to_vfo(response);
        return (int)*vfo;
    }
    return -1;
}
