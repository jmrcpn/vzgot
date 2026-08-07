# vzgot Manual & Command Reference

The `vzgot` utility manages container lifecycles, execution modes, status monitoring, and host migrations.

---

## 1. Synopsis

```bash
vzgot [-c confdir] [-d debug] [-f] [-h] [-p] [-v] <action_word> <name> [starter]
```

---

## 2. Global Options

| Flag | Argument | Description | Default |
| :--- | :--- | :--- | :--- |
| **`-c`** | `confdir` | Use alternative configuration directory | `/etc/vzgot/` |
| **`-d`** | `debug` | Set debug logging level | Disabled |
| **`-f`** | — | Run container in foreground mode | Background |
| **`-h`** | — | Display built-in help message and exit | — |
| **`-p`** | — | Start container in privileged mode | Unprivileged |
| **`-v`** | — | Enable verbose debugging output | Disabled |

---

## 3. Actions (`action_word`)

| Action Command | Modifiers | Description |
| :--- | :--- | :--- |
| **`boot`** | — | Boot target container |
| **`onboot`** | — | Enable container autostart on Host boot sequence |
| **`offboot`** | — | Disable container autostart on Host boot sequence |
| **`reboot`** | — | Restart container (shut down then boot) |
| **`shutdown`** | — | Gracefully shut down container |
| **`create`** | — | Create container from scratch |
| **`destroy`** | — | Destroy container filesystem |
| **`enter`** | — | Open an interactive shell inside container |
| **`exec`** | — | Execute a command inside container |
| **`freeze`** | — | Extract container critical configuration |
| **`movefrom`** | — | Move a running container from a remote Host and start it locally |
| **`online`** | `[-r s]` | List online containers uptime (refresh every `s` seconds) |
| **`status`** | `[-r]` | Display container status (refresh every 1 second with `-r`) |

---

## 4. Arguments

* **`<name>`**: Target container name (or FQDN / IP specification during creation).
* **`[starter]`** *(Optional)*: Program to execute on boot (default: `/bin/init`).
