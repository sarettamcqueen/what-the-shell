# What the Shell 🐚

> Una shell minimale con filesystem virtuale user-space, scritta in C.  
> Progetto per il corso di **Complementi di Sistemi Operativi** – Sapienza Università di Roma.

---

## 🧠 Descrizione

"What the Shell" è una shell interattiva che opera su un **filesystem simulato** persistente, implementato all'interno di un file binario (`fs.img`) che simula un disco.  
Tutti i comandi della shell agiscono **esclusivamente all’interno del filesystem virtuale**, gestito in memoria tramite `mmap`.

---

## ✨ Funzionalità previste

- [x] Inizializzazione del filesystem (`format`)
- [ ] Navigazione tra directory (`cd`, `ls`)
- [ ] Creazione file e directory (`touch`, `mkdir`)
- [ ] Lettura e scrittura su file (`cat`, `append <file> <testo>`)
- [ ] Rimozione di file e directory (`rm`)
- [ ] Salvataggio e uscita (`close`)

---
