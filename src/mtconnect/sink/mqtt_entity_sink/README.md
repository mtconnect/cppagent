# MTConnect MQTT Entity Sink

## Overview

The MQTT Entity Sink publishes MTConnect observations to an external MQTT broker in a flat, per-data-item topic structure compatible with MTConnect.NET's MQTT Relay Entity format.

## Features

- **Real-time streaming** of observations
- **Flat topic structure**: `MTConnect/Devices/[device-uuid]/Observations/[dataItemId]`
- **JSON format** with full observation metadata
- **ISO 8601 timestamps**
- **Supports SAMPLE, EVENT, and CONDITION observations**
- **Last Will Testament** for availability
- **TLS/SSL support** for secure communication
- **Configurable QoS levels** for MQTT message delivery
- **Message retention** support for new subscribers

## Quick Start

### 1. Configuration

Add to `agent.cfg`:

```properties
Sinks {
    MqttEntitySink {
        MqttHost = localhost
        MqttPort = 1883
        MqttTls = false

        # QoS levels (string values):
        # at_most_once = Fire and forget
        # at_least_once = Acknowledged delivery (default)
        # exactly_once = Guaranteed delivery
        MqttQOS = at_least_once

        # Retain last message on broker for new subscribers
        MqttRetain = false
    }
}
```

## Configuration Options

### Connection Settings

| Parameter         | Type    | Default      | Description                                   |
|-------------------|---------|-------------|-----------------------------------------------|
| **MqttHost**      | string  | `127.0.0.1` | MQTT broker hostname or IP address            |
| **MqttPort**      | integer | `1883`      | MQTT broker port (1883 for TCP, 8883 for TLS) |
| **MqttTls**       | boolean | `false`     | Enable TLS encryption                         |
| **MqttClientId**  | string  | auto-gen     | MQTT client identifier (unique per connection) |

### Authentication

| Parameter         | Type    | Default | Description                                   |
|-------------------|---------|---------|-----------------------------------------------|
| **MqttUserName**  | string  | -       | Username for MQTT authentication (optional)    |
| **MqttPassword**  | string  | -       | Password for MQTT authentication (optional)    |

### TLS/SSL Settings

| Parameter         | Type    | Default | Description                                   |
|-------------------|---------|---------|-----------------------------------------------|
| **MqttCaCert**    | string  | -       | Path to CA certificate for TLS verification   |
| **MqttCert**      | string  | -       | Path to client certificate for mutual TLS     |
| **MqttPrivateKey**| string  | -       | Path to client private key                    |

### Topic Configuration

| Parameter                | Type    | Default                                   | Description                                  |
|--------------------------|---------|-------------------------------------------|----------------------------------------------|
| **ObservationTopicPrefix**| string | `MTConnect/Devices/[device]/Observations` | Topic prefix for observations. `[device]` is replaced with device UUID |
| **DeviceTopicPrefix**    | string  | `MTConnect/Probe/[device]`                | Topic prefix for device models               |
| **AssetTopicPrefix**     | string  | `MTConnect/Asset/[device]`                | Topic prefix for assets                      |
| **MqttLastWillTopic**    | string  | `MTConnect/Probe/[device]/Availability`   | Last Will Testament topic for availability   |

### MQTT Protocol Settings

| Parameter     | Type    | Default         | Description                                 |
|---------------|---------|-----------------|---------------------------------------------|
| **MqttQOS**   | string  | `at_least_once` | Quality of Service level for MQTT messages  |
| **MqttRetain**| boolean | `false`         | Retain messages on the broker for new subscribers |

#### MqttQOS Values

The Quality of Service (QoS) level determines how messages are delivered:

- **at_most_once** (Fire and forget)
  - Fastest delivery
  - No acknowledgment required
  - Message may be lost if network fails
  - **Use case**: High-frequency data where occasional loss is acceptable

- **at_least_once** (Default)
  - Guaranteed delivery with acknowledgment
  - Messages may be duplicated
  - Balance of reliability and performance
  - **Use case**: General-purpose MTConnect data streaming

- **exactly_once**
  - Guaranteed exactly-once delivery
  - Highest reliability, slowest performance
  - Four-way handshake protocol
  - **Use case**: Critical alarms, conditions, or important events

```properties
# Example: High-reliability configuration for critical data
MqttQOS = exactly_once
```

#### MqttRetain Flag

The retain flag determines if the broker stores the last message for each topic:

- **false** (Default)
  - Messages are not retained
  - New subscribers only receive future messages
  - Lower broker storage requirements
  - **Use case**: High-frequency streaming data

- **true**
  - Broker stores the last message per topic
  - New subscribers immediately receive last known value
  - Useful for status and current values
  - **Use case**: Availability, current state, alarms

```properties
# Example: Retain messages for immediate status updates
MqttRetain = true
```

**⚠️ Important Notes:**
- Retained messages persist on the broker until explicitly cleared
- Use retention sparingly with high-frequency data to avoid broker storage issues
- Retained availability messages help dashboards show immediate device status

## Complete Configuration Example

```properties
Sinks {
    MqttEntitySink {
        MqttHost = localhost
        MqttPort = 1883
        MqttTls = false
        MqttQOS = at_least_once
        MqttRetain = false
        ObservationTopicPrefix = MTConnect/Devices/[device]/Observations
        DeviceTopicPrefix = MTConnect/Probe/[device]
        AssetTopicPrefix = MTConnect/Asset/[device]
        MqttLastWillTopic = MTConnect/Probe/[device]/Availability
    }
}
```

## Message Format

### SAMPLE Observation
```json
{
  "dataItemId": "Xact",
  "name": "X_actual",
  "type": "POSITION",
  "subType": "ACTUAL",
  "category": "SAMPLE",
  "timestamp": "2025-10-20T15:30:45.123456Z",
  "result": "150.500000",
  "sequence": 1234
}
```

### EVENT Observation
```json
{
  "dataItemId": "mode",
  "name": "ControllerMode",
  "type": "CONTROLLER_MODE",
  "category": "EVENT",
  "timestamp": "2025-10-20T15:30:46.000000Z",
  "result": "AUTOMATIC",
  "sequence": 1235
}
```

### CONDITION Observation
```json
{
  "dataItemId": "system",
  "name": "SystemCondition",
  "type": "SYSTEM",
  "category": "CONDITION",
  "level": "FAULT",
  "nativeCode": "E001",
  "conditionId": "E001",
  "message": "Hydraulic pressure low",
  "timestamp": "2025-10-20T15:30:47.000000Z",
  "sequence": 1236
}
```

## Topic Structure

```
MTConnect/
├── Devices/
│   └── {device-uuid}/
│       └── Observations/
│           ├── {dataItemId1}  → Observation JSON
│           ├── {dataItemId2}  → Observation JSON
│           └── ...
└── Probe/
    └── {device-uuid}/
        └── Availability        → AVAILABLE/UNAVAILABLE (LWT)
```

**Example Topics:**
```
MTConnect/Devices/OKUMA.123456/Observations/Xact
MTConnect/Devices/OKUMA.123456/Observations/avail
MTConnect/Probe/OKUMA.123456/Availability
```

## Performance Considerations

### QoS Impact

- **at_most_once**: ~2x faster than at_least_once, best for high-frequency sampling data
- **at_least_once**: Balanced performance, suitable for most use cases
- **exactly_once**: ~2x slower than at_least_once, use only for critical data

### Retain Flag Impact

- **Without Retain**: Minimal broker storage, suitable for streaming
- **With Retain**: Increases broker storage per topic, use selectively for:
  - Availability status
  - Current operating mode
  - Alarm states
  - Device health indicators

**💡 Recommendation**: Use `MqttRetain = true` only for low-frequency, stateful data.

---
