#!/bin/bash

# Contributed by a P1P2MQTT user - many thanks!

# Variables
BROKER="IP_ADDRESS"      # The address of the MQTT broker (adjust if needed)
TOPIC="P1P2/P/P1P2MQTT/bridge0/#"      # The MQTT topic to subscribe to
USERNAME="user"                # Username for broker (if needed)
PASSWORD="password"             # Password for broker (if needed)

# Declare an associative array to hold previous values for each topic
declare -A PREVIOUS_VALUES

# Initialize a counter
change_count=0

# Function to handle incoming MQTT messages
handle_message() {
    local topic=$1
    local new_value=$2

    # Check if the topic has a previous value stored
    if [ "${PREVIOUS_VALUES[$topic]}" != "$new_value" ]; then
        if [ -n "${PREVIOUS_VALUES[$topic]}" ]; then
            # Print the topic and values only if they are different
            printf "%-70s |  %-10s | %-10s |\n" "$topic" "${PREVIOUS_VALUES[$topic]}" "$new_value"
            # Increment the change counter
            ((change_count++))

            # Check if the change counter has reached 10 to show the header again
            if (( change_count % 10 == 0 )); then
                echo "---------------------------------------------------------------------------------------------------"
                echo "                                                                       |   Previous  |    New     |"
                echo "                    Topic                                              |     Value   |   Value    |"
                echo "---------------------------------------------------------------------------------------------------"
            fi
        fi
        PREVIOUS_VALUES[$topic]=$new_value  # Update the previous value for this topic
    fi
}

# Display Header
echo "                                                                       |   Previous  |    New     |"
echo "                    Topic                                              |     Value   |   Value    |"
echo "---------------------------------------------------------------------------------------------------"

# Subscribe to the topic and process messages
mosquitto_sub -h "$BROKER" -t "$TOPIC" -u "$USERNAME" -P "$PASSWORD" -v |
while read -r payload; do
    # Split the payload into topic and value
    IFS=' ' read -r topic new_value <<< "$payload"
    # Call the function with the topic and the new value
    handle_message "$topic" "$new_value"
done
