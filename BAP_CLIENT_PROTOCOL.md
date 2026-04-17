# GT Touch BAP Client Protocol

This document describes the protocol the GT Touch firmware uses over the USB CDC serial link so a host or server can feed live miner data to the display and accept user-issued control changes.

## Transport

- Physical link: USB CDC ACM serial
- Payload type: ASCII line protocol
- Line ending sent by GT Touch: `\r\n`
- Line endings accepted by GT Touch: `\r\n` or `\n`
- Host tip: open the CDC serial port and assert `DTR`

The firmware treats the USB CDC connection as its BAP transport. There is no Wi-Fi or USB-ECM path anymore.

## Message Format

There are two wire formats in use:

### 1. Subscription lines from GT Touch

Subscriptions are sent without a checksum:

```text
$BAP,SUB,<parameter>\r\n
```

Example:

```text
$BAP,SUB,hashrate
```

### 2. Checked messages

Requests, settings, and all messages sent back to the GT Touch use the checksum form:

```text
$BAP,<CMD>,<parameter>,<value>*<checksum>\r\n
```

Or, for requests with no value:

```text
$BAP,REQ,<parameter>*<checksum>\r\n
```

The checksum is an uppercase 2-digit hex XOR of every byte between `$` and `*`.

Example:

```text
Body: BAP,REQ,systemInfo
XOR:  3E
Line: $BAP,REQ,systemInfo*3E
```

## What The GT Touch Sends

About 7 seconds after boot, the GT Touch sends this startup sequence with roughly 100 ms between lines:

```text
$BAP,SUB,hashrate
$BAP,SUB,temperature
$BAP,SUB,power
$BAP,SUB,fan_speed
$BAP,SUB,shares
$BAP,SUB,best_difficulty
$BAP,SUB,block_height
$BAP,REQ,systemInfo*3E
```

If the display does not receive any incoming BAP traffic for about 12 seconds after it has been talking to the host, it resets its connection state and sends the same startup sequence again.

## What The Host Should Send Back

The GT Touch only handles incoming `RES` and `CMD` messages. In practice, `RES` is what matters for normal server integration.

All incoming lines must include a checksum.

### Subscription Mapping

The subscription names are not always the same as the response parameter names. This table shows what the GT Touch asks for and what it actually accepts back:

| GT Touch sends | Host should answer with |
| --- | --- |
| `SUB hashrate` | `RES hashrate` |
| `SUB temperature` | `RES chipTemp` |
| `SUB power` | `RES power` |
| `SUB fan_speed` | `RES fan_speed` |
| `SUB shares` | `RES shares` |
| `SUB best_difficulty` | `RES best_difficulty` |
| `SUB block_height` | `RES block_height` |
| `REQ systemInfo` | one or more `RES` lines listed below |

### `systemInfo` Response Set

When the display requests `systemInfo`, send whichever of these you have available:

- `deviceModel`
- `asicModel`
- `pool`
- `poolPort`
- `poolUser`
- `mode` optional
- `voltage` optional

The current UI uses these fields as follows:

- `deviceModel`: shown in the hardware popup
- `asicModel`: shown in the hardware popup
- `pool`: shown in the pool popup
- `poolPort`: shown in the pool popup
- `poolUser`: shown in the pool popup
- `mode`: accepted but not currently displayed
- `voltage`: logged but not currently displayed

## Expected Value Formats

Send plain strings without units unless noted otherwise.

| Parameter | Suggested value format | Notes |
| --- | --- | --- |
| `hashrate` | `1234.56` | Displayed directly, also parsed as a float for efficiency/night mode |
| `chipTemp` | `54.25` | Parsed as float and redisplayed as `54.25C` |
| `power` | `18.4` | Display shows `W` automatically |
| `fan_speed` | `6120` | Display shows `RPM` automatically |
| `shares` | `42` | Displayed directly |
| `best_difficulty` | `1024` | Displayed directly |
| `block_height` | `891234` | Parsed as integer |
| `deviceModel` | `Gamma` | Free-form string |
| `asicModel` | `BM1370` | Free-form string |
| `pool` | `stratum+tcp://public-pool.io` | Free-form string |
| `poolPort` | `3333` | Free-form string or integer-as-string |
| `poolUser` | `my-worker` | Free-form string |
| `mode` | `normal` | Optional |
| `voltage` | `1200` or `1200.00` | Optional |

## Example Host Responses

```text
$BAP,RES,hashrate,1234.56*02
$BAP,RES,chipTemp,54.25*2D
$BAP,RES,power,18.4*57
$BAP,RES,fan_speed,6120*6F
$BAP,RES,shares,42*23
$BAP,RES,best_difficulty,1024*70
$BAP,RES,block_height,891234*17
$BAP,RES,deviceModel,Gamma*2B
$BAP,RES,asicModel,BM1370*66
$BAP,RES,pool,stratum+tcp://public-pool.io*31
$BAP,RES,poolPort,3333*1E
$BAP,RES,poolUser,my-worker*39
```

## Control Commands Sent By The GT Touch

When the user changes settings on the display, the GT Touch sends `SET` messages upstream.

### Performance Presets

Low:

```text
$BAP,SET,frequency,575*6E
$BAP,SET,asic_voltage,1160.00*30
```

Medium:

```text
$BAP,SET,frequency,600*6F
$BAP,SET,asic_voltage,1200.00*35
```

High:

```text
$BAP,SET,frequency,655*6F
$BAP,SET,asic_voltage,1200.00*35
```

### Fan Control

Automatic fan control:

```text
$BAP,SET,auto_fan,1*35
```

Manual fan speed example:

```text
$BAP,SET,fan_speed,75*6E
```

The current firmware does not require an explicit acknowledgement for these `SET` commands. The host should simply apply them.

## Parser Constraints

These limits come from the current firmware parser:

- command string: up to 15 characters
- parameter: up to 31 characters
- value: up to 63 characters
- full outbound message buffer on the GT Touch: 256 bytes

Values may contain commas, but they must not contain `*` because `*` terminates the checksum section.

Unknown incoming parameters are ignored after being logged.

## Minimal Server Behavior

For a stable integration, the host should:

1. Open the GT Touch CDC serial port and assert `DTR`.
2. Read line-oriented ASCII messages.
3. Record the active subscriptions.
4. Answer `REQ systemInfo` with the available `RES` fields.
5. Stream periodic `RES` updates for the subscribed live metrics.
6. Continue sending at least some valid traffic more often than every 12 seconds so the display does not reconnect and re-subscribe.
7. Apply incoming `SET` commands for frequency, ASIC voltage, and fan control.

## Reference Flow

```text
GT Touch -> $BAP,SUB,hashrate
GT Touch -> $BAP,SUB,temperature
GT Touch -> $BAP,SUB,power
GT Touch -> $BAP,SUB,fan_speed
GT Touch -> $BAP,SUB,shares
GT Touch -> $BAP,SUB,best_difficulty
GT Touch -> $BAP,SUB,block_height
GT Touch -> $BAP,REQ,systemInfo*3E

Host -> $BAP,RES,deviceModel,Gamma*2B
Host -> $BAP,RES,asicModel,BM1370*66
Host -> $BAP,RES,pool,stratum+tcp://public-pool.io*31
Host -> $BAP,RES,poolPort,3333*1E
Host -> $BAP,RES,poolUser,my-worker*39

Host -> $BAP,RES,hashrate,1234.56*02
Host -> $BAP,RES,chipTemp,54.25*2D
Host -> $BAP,RES,power,18.4*57
Host -> $BAP,RES,fan_speed,6120*6F
Host -> $BAP,RES,shares,42*23
Host -> $BAP,RES,best_difficulty,1024*70
Host -> $BAP,RES,block_height,891234*17
```
