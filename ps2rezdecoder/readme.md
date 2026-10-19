# PS2 Rez Decoder and Extractor

Extracts PS2 lithtech rez files (e.g. `LITH_PS2.REZ` from *No One Lives Forever*).

The rez only contains hashes, so if you're missing a specific file, you can add it into the file list.  
The application will hash that filename/path and check it against the hashes in the rez file.  
If it matches, it'll extract it for you!

**Requires Visual Studio 2019 or any C++14 compatible compiler.**

---

## Improvements in this version

- Only the requested language block is loaded → **much lower RAM usage**
- Hash map for O(1) file lookup
- Single reusable buffer for extraction (no memory leaks / many small allocations)
- Progress display while extracting
- **New mode `all`**: extract every file of a block (even without known names)
- Cleaner path handling
- Still 100 % C++14 / VS 2019 compatible

---

## Usage

```bash
ps2rezdecoder [options]
```

### Parameters

| Parameter       | Short | Default              | Description                                      |
|-----------------|-------|----------------------|--------------------------------------------------|
| `--help`        | `-h`  | –                    | Shows help                                       |
| `--file`        | `-f`  | `file list.txt`      | Path to the file list (only for `extract` mode)  |
| `--rez`         | `-r`  | `LITH_PS2.REZ`       | Path to the REZ file                             |
| `--block`       | `-b`  | `0`                  | Language block to extract (see table below)      |
| `--mode`        | `-m`  | `extract`            | Mode: `extract` | `all` | `hash`                  |

### Modes

| Mode      | Description                                                                 |
|-----------|-----------------------------------------------------------------------------|
| `extract` | Extract only files whose names are in the file list (original behaviour)    |
| `all`     | Extract **every** file of the chosen block → saved as `hash_XXXXXXXX.bin`   |
| `hash`    | Just calculate the hash of a single filename                                |

### Examples

```bash
# Extract English (default – known names only)
ps2rezdecoder

# Extract German using the full file list
ps2rezdecoder -b 2 -f "file list - full.txt"

# Extract ALL files from the German block (recommended!)
ps2rezdecoder -b 2 -m all

# Extract ALL files from the French block
ps2rezdecoder -b 1 -m all

# Just calculate a hash
ps2rezdecoder -m hash -f "some/path/file.ltb"
```

---

## Language Blocks

The `LITH_PS2.REZ` file is split into **5 blocks**.  
Each block contains the files for one language (mainly the `.LIT` voice packages).

| Block | Language     |
|-------|--------------|
| **0** | English      |
| **1** | French       |
| **2** | German       |
| **3** | Spanish      |
| **4** | Italian      |

> **Note:** This mapping comes from community analysis (ZenHAX).

---

## File Lists

There are three different file lists included:

| File                        | Approx. entries | Description                                      | Recommendation                     |
|-----------------------------|-----------------|--------------------------------------------------|------------------------------------|
| `file list.txt`             | medium          | Default list                                     | Everyday use                       |
| `file list - clean.txt`     | ~4 700          | Cleaned / slightly reduced list                  | When you want fewer files          |
| `file list - full.txt`      | ~38 900         | Almost complete list of known files              | Maximum known-name extraction      |

**Tipp:** Für andere Sprachen am besten `-m all` verwenden – dann brauchst du keine Dateiliste.

---

## Special Thanks

- aluigi and this thread https://zenhax.com/viewtopic.php?t=11139 for the original script.
- LithFAQ community.
- Monolith/Sierra for leaving debug symbols in LTIOREZ.IRX.
- My partner for not being mad I woke her up at 2:30am that one night going to bed.
