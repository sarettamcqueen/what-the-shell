# What the Shell 🐚

> A minimal shell with a user-space virtual filesystem, written in C.  
> Project for the course **Complementi di Sistemi Operativi** at Sapienza University of Rome.

---

## 🧠 Description

**What the Shell** is an interactive shell operating on a **persistent simulated filesystem**, implemented inside a binary file (`fs.img`) acting as a virtual disk.  
All shell commands operate **exclusively within the virtual filesystem**, which is managed in memory using `mmap`.

---

## ✨ Planned Features

- [ ] Filesystem initialization (`format`)
- [ ] Directory navigation (`cd`, `ls`)
- [ ] File and directory creation (`touch`, `mkdir`)
- [ ] File reading and writing (`cat`, `append <file> <text>`)
- [ ] File and directory removal (`rm`)
- [ ] Save and exit (`close`)
