# Student Expense Tracker

A full-stack student expense tracker with a **C backend** and **vanilla HTML/CSS/JS frontend**.

## 🚀 Quick Start

### Prerequisites
You need **MinGW-w64 GCC** installed on Windows. 

**Install with winget (recommended):**
```bash
winget install -e --id MSYS2.MSYS2
```
Then in MSYS2 terminal:
```bash
pacman -S mingw-w64-ucrt-x86_64-gcc
```
Add `C:\msys64\ucrt64\bin` to your PATH.

**OR install TDM-GCC (simpler):**
Download from https://jmeubank.github.io/tdm-gcc/ — it automatically adds to PATH.

### Build & Run
```bash
cd "varsha c project"
make
expense_tracker.exe
```

Open **http://localhost:8080** in your browser.

## 📁 Project Structure
```
├── main.c              ← HTTP server + route dispatcher
├── db.c / db.h         ← SQLite setup, schema, queries
├── expenses.c / .h     ← Expense CRUD handlers
├── budgets.c / .h      ← Budget logic + alert checks
├── reports.c / .h      ← Summary + CSV generation
├── recurring.c / .h    ← Recurring expense processor
├── categories.c / .h   ← Category seeding + fetch
├── audit.c / .h        ← Audit log writer
├── lib/                ← Vendored libraries (Civetweb, cJSON, SQLite3)
├── public/             ← Frontend (HTML, CSS, JS)
│   ├── index.html
│   ├── style.css
│   └── app.js
├── uploads/            ← Receipt images
└── Makefile
```

## ✨ Features
- 📊 Dashboard with spending charts (Chart.js)
- ➕ Add/edit/delete expenses
- 📈 Analytics with daily spending & category breakdown
- 💰 Per-category budget limits with alerts
- 🔄 Recurring expenses (daily/weekly/monthly)
- 📎 Receipt image upload
- 📥 CSV export
- 📋 Audit log
- 🌙 Dark/Light mode
- 📱 Mobile-first responsive design

## 🛠 Tech Stack
- **Backend:** C + Civetweb (HTTP server) + SQLite3 (database) + cJSON (JSON)
- **Frontend:** Vanilla HTML/CSS/JS + Chart.js
- **No npm, no frameworks, no external services**
