# PS2 Rez Decoder and Extractor

Extracts PS2 LithTech REZ files (e.g. `LITH_PS2.REZ` from *No One Lives Forever*).

The REZ only contains hashes.  
Add any missing filename/path to the file list – the tool hashes it and extracts the matching entry if present.

**Requires Visual Studio 2019 or any C++14 compatible compiler.**

---

## Improvements (V1.1)

- Hash map for O(1) file lookup
- Single reusable extraction buffer (no memory leaks / many small allocations)
- Cleaner path handling
- Language group selection (`-l 0..4`) with correct interleaved table logic
- Detailed extraction summary
- Optional dump of unknown hashes as `.bin` files (`-u`) + manifest
- Diagnostic file `not_found.txt`
- Still 100 % C++14 / VS 2019 compatible

---

## Usage

```bash
ps2rezdecoder [options]
```

### Parameters

| Parameter     | Short | Default          | Description                                      |
|---------------|-------|------------------|--------------------------------------------------|
| `--help`      | `-h`  | –                | Shows help                                       |
| `--file`      | `-f`  | `file list.txt`  | Path to the file list (or single file in hash mode) |
| `--rez`       | `-r`  | `LITH_PS2.REZ`   | Path to the REZ file                             |
| `--language`  | `-l`  | `0`              | Language group 0–4 (see table below)             |
| `--mode`      | `-m`  | `extract`        | Mode: `extract` or `hash`                        |
| `--unknowns`  | `-u`  | off              | Also extract unknown hashes as `.bin` files      |

### Examples

```bash
# Extract English (default language group 0)
ps2rezdecoder

# Extract German language group
ps2rezdecoder -l 2

# Extract French + dump all unknown files
ps2rezdecoder -l 1 -u

# Just calculate a hash
ps2rezdecoder -m hash -f "some/path/file.ltb"
```

---

## Language Groups

The REZ contains multiple tables that are interleaved by language group:

| Group | Tables          | Typical content |
|-------|-----------------|-----------------|
| **0** | 1, 6, 11, …     | English         |
| **1** | 2, 7, 12, …     | French          |
| **2** | 3, 8, 13, …     | German          |
| **3** | 4, 9, 14, …     | Spanish         |
| **4** | 5, 10, 15, …    | Italian         |

> Mapping based on community analysis (ZenHAX) and further testing.

---

## File List

Only one cleaned file list is included:

| File            | Description                          | Recommendation     |
|-----------------|--------------------------------------|--------------------|
| `file list.txt` | Cleaned list of known filenames      | Default / all known |

With `-u` you can additionally dump every remaining unknown hash as `unknown_XXXXXXXX.bin` (plus a `unknown_hashlist.txt` manifest in `out/unknown/`).

---

## Output

- Known files are written with their original paths under `out/`.
- Unknown files (when `-u` is used) go into `out/unknown/`.
- A diagnostic file `not_found.txt` lists every entry from the file list that could not be found (`HASH_NOT_PRESENT` or `NO_LANGUAGE_ENTRY`).

---

## Special Thanks

- aluigi and this thread https://zenhax.com/viewtopic.php?t=11139 for the original script.
- LithFAQ community.
- Monolith/Sierra for leaving debug symbols in LTIOREZ.IRX.
- My partner for not being mad I woke her up at 2:30am that one night going to bed.
