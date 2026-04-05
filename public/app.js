/* ============================================
   Student Expense Tracker — App.js
   SPA Router, API Layer, Views, Charts
   ============================================ */

(function () {
    'use strict';

    /* ------------------------------------------
       DEFAULT CATEGORIES (fallback when API is unavailable)
       ------------------------------------------ */
    const DEFAULT_CATEGORIES = [
        { id: 1, name: 'Food',          color: '#EF4444', icon: '🍔' },
        { id: 2, name: 'Transport',     color: '#F59E0B', icon: '🚌' },
        { id: 3, name: 'Books',         color: '#3B82F6', icon: '📚' },
        { id: 4, name: 'Hostel',        color: '#8B5CF6', icon: '🏠' },
        { id: 5, name: 'Health',        color: '#10B981', icon: '💊' },
        { id: 6, name: 'Entertainment', color: '#EC4899', icon: '🎮' },
    ];

    /* ------------------------------------------
       STATE
       ------------------------------------------ */
    const state = {
        categories: [],
        currentMonth: new Date(),
        editingExpense: null,
    };

    /* ------------------------------------------
       API LAYER
       ------------------------------------------ */
    const API = {
        async get(url) {
            const res = await fetch(url);
            return res.json();
        },
        async post(url, data) {
            const res = await fetch(url, {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify(data),
            });
            return res.json();
        },
        async put(url, data) {
            const res = await fetch(url, {
                method: 'PUT',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify(data),
            });
            return res.json();
        },
        async del(url) {
            const res = await fetch(url, { method: 'DELETE' });
            return res.json();
        },

        getCategories: () => API.get('/api/categories'),
        getExpenses: (month) => API.get(`/api/expenses${month ? '?month=' + month : ''}`),
        getExpenseById: (id) => API.get(`/api/expenses/${id}`),
        addExpense: (data) => API.post('/api/expenses', data),
        updateExpense: (id, data) => API.put(`/api/expenses/${id}`, data),
        deleteExpense: (id) => API.del(`/api/expenses/${id}`),
        getSummary: (month) => API.get(`/api/expenses/summary${month ? '?month=' + month : ''}`),
        getBudgets: () => API.get('/api/budgets'),
        setBudget: (data) => API.post('/api/budgets', data),
        getMonthlyReport: (month) => API.get(`/api/reports/monthly${month ? '?month=' + month : ''}`),
        processRecurring: () => API.post('/api/recurring/process', {}),
        getAuditLog: () => API.get('/api/audit'),
    };

    /* ------------------------------------------
       UTILITY HELPERS
       ------------------------------------------ */
    function $(sel) { return document.querySelector(sel); }
    function $$(sel) { return document.querySelectorAll(sel); }

    function formatCurrency(n) {
        return '₹' + Number(n || 0).toLocaleString('en-IN', { minimumFractionDigits: 0, maximumFractionDigits: 0 });
    }

    function formatDate(dateStr) {
        const d = new Date(dateStr + 'T00:00:00');
        return d.toLocaleDateString('en-IN', { day: 'numeric', month: 'short' });
    }

    function getMonthStr(date) {
        const y = date.getFullYear();
        const m = String(date.getMonth() + 1).padStart(2, '0');
        return `${y}-${m}`;
    }

    function getMonthLabel(date) {
        return date.toLocaleDateString('en-US', { month: 'long', year: 'numeric' });
    }

    function getTodayStr() {
        const d = new Date();
        return `${d.getFullYear()}-${String(d.getMonth() + 1).padStart(2, '0')}-${String(d.getDate()).padStart(2, '0')}`;
    }

    function showToast(msg, type = '') {
        const container = $('#toast-container');
        const toast = document.createElement('div');
        toast.className = 'toast ' + type;
        toast.textContent = msg;
        container.appendChild(toast);
        setTimeout(() => toast.remove(), 3000);
    }

    /* ------------------------------------------
       THEME MANAGEMENT
       ------------------------------------------ */
    function initTheme() {
        const dark = localStorage.getItem('theme') === 'dark';
        document.body.classList.toggle('dark', dark);
        updateThemeControls(dark);
    }

    function toggleTheme() {
        const isDark = document.body.classList.toggle('dark');
        localStorage.setItem('theme', isDark ? 'dark' : 'light');
        updateThemeControls(isDark);
    }

    function updateThemeControls(isDark) {
        const icon = isDark ? '☀️' : '🌙';
        const sidebarBtn = $('#theme-toggle-sidebar');
        if (sidebarBtn) sidebarBtn.querySelector('.theme-icon').textContent = icon;

        const settingsToggle = $('#theme-toggle-settings');
        if (settingsToggle) settingsToggle.checked = isDark;
    }

    /* ------------------------------------------
       ROUTER
       ------------------------------------------ */
    function navigate(viewName) {
        $$('.view').forEach(v => v.classList.remove('active'));
        $$('.nav-link, .tab-link').forEach(l => l.classList.remove('active'));

        const view = $(`#view-${viewName}`);
        if (view) view.classList.add('active');

        $$(`[data-view="${viewName}"]`).forEach(l => l.classList.add('active'));

        // Load view data
        switch (viewName) {
            case 'dashboard': loadDashboard(); break;
            case 'add': loadAddView(); break;
            case 'analytics': loadAnalytics(); break;
            case 'settings': loadSettings(); break;
        }
    }

    function handleHashChange() {
        const hash = location.hash.replace('#', '') || 'dashboard';

        // Check if editing: #edit/123
        if (hash.startsWith('edit/')) {
            const id = parseInt(hash.split('/')[1]);
            if (id > 0) {
                state.editingExpense = id;
                navigate('add');
                return;
            }
        }

        state.editingExpense = null;
        navigate(hash);
    }

    /* ------------------------------------------
       DASHBOARD VIEW
       ------------------------------------------ */
    let doughnutChart = null;

    async function loadDashboard() {
        const month = getMonthStr(new Date());
        $('#dashboard-month-label').textContent = getMonthLabel(new Date());

        try {
            const [summary, budgets, expenses] = await Promise.all([
                API.getSummary(month),
                API.getBudgets(),
                API.getExpenses(month),
            ]);

            // Summary cards
            const totalSpent = summary.total || 0;
            const totalBudget = budgets.reduce((s, b) => s + (b.monthly_limit || 0), 0);
            const remaining = totalBudget - totalSpent;

            $('#total-spent').textContent = formatCurrency(totalSpent);
            $('#total-budget').textContent = formatCurrency(totalBudget);
            $('#total-remaining').textContent = formatCurrency(remaining);
            $('#total-remaining').style.color = remaining < 0 ? 'var(--danger)' : '';

            // Budget alerts
            renderBudgetAlerts(budgets);

            // Doughnut chart
            renderDoughnutChart(summary.categories || []);

            // Recent expenses (last 5)
            renderRecentExpenses(expenses.slice(0, 5));
        } catch (err) {
            console.error('Dashboard load error:', err);
        }
    }

    function renderDoughnutChart(categories) {
        const canvas = $('#dashboard-doughnut-chart');
        const ctx = canvas.getContext('2d');

        const hasData = categories.some(c => c.total > 0);

        if (doughnutChart) doughnutChart.destroy();

        if (!hasData) {
            doughnutChart = new Chart(ctx, {
                type: 'doughnut',
                data: {
                    labels: ['No expenses yet'],
                    datasets: [{ data: [1], backgroundColor: ['#E2E8F0'], borderWidth: 0 }]
                },
                options: {
                    responsive: true,
                    maintainAspectRatio: true,
                    cutout: '65%',
                    plugins: { legend: { display: false } }
                }
            });
            return;
        }

        const filtered = categories.filter(c => c.total > 0);
        doughnutChart = new Chart(ctx, {
            type: 'doughnut',
            data: {
                labels: filtered.map(c => `${c.icon} ${c.name}`),
                datasets: [{
                    data: filtered.map(c => c.total),
                    backgroundColor: filtered.map(c => c.color),
                    borderWidth: 2,
                    borderColor: getComputedStyle(document.body).getPropertyValue('--bg-secondary').trim(),
                    hoverOffset: 8,
                }]
            },
            options: {
                responsive: true,
                maintainAspectRatio: true,
                cutout: '62%',
                plugins: {
                    legend: {
                        position: 'bottom',
                        labels: {
                            padding: 14,
                            usePointStyle: true,
                            pointStyleWidth: 12,
                            font: { family: 'Inter', size: 12, weight: '500' },
                            color: getComputedStyle(document.body).getPropertyValue('--text-secondary').trim(),
                        }
                    },
                    tooltip: {
                        callbacks: {
                            label: (ctx) => ` ${ctx.label}: ${formatCurrency(ctx.raw)}`
                        }
                    }
                }
            }
        });
    }

    function renderBudgetAlerts(budgets) {
        const container = $('#budget-alerts');
        container.innerHTML = '';

        const dismissed = JSON.parse(localStorage.getItem('dismissedAlerts') || '{}');

        budgets.forEach(b => {
            if (b.monthly_limit <= 0) return;
            if (b.status === 'ok') return;

            const key = `${b.category_id}-${getMonthStr(new Date())}`;
            if (dismissed[key]) return;

            const isExceeded = b.status === 'exceeded';
            const pct = Math.round(b.percentage);

            const div = document.createElement('div');
            div.className = `alert-banner ${isExceeded ? 'alert-danger' : 'alert-warning'}`;
            div.innerHTML = `
                <span>${isExceeded ? '🚨' : '⚠️'}</span>
                <span><strong>${b.icon} ${b.name}</strong> — ${pct}% of budget used (${formatCurrency(b.spent)} / ${formatCurrency(b.monthly_limit)})</span>
                <button class="alert-dismiss" data-key="${key}">✕</button>
            `;
            container.appendChild(div);
        });

        container.querySelectorAll('.alert-dismiss').forEach(btn => {
            btn.addEventListener('click', () => {
                const key = btn.dataset.key;
                const d = JSON.parse(localStorage.getItem('dismissedAlerts') || '{}');
                d[key] = true;
                localStorage.setItem('dismissedAlerts', JSON.stringify(d));
                btn.closest('.alert-banner').remove();
            });
        });
    }

    function renderRecentExpenses(expenses) {
        const list = $('#recent-expenses-list');
        if (!expenses.length) {
            list.innerHTML = `
                <div class="empty-state">
                    <span class="empty-state-icon">📝</span>
                    <p class="empty-state-text">No expenses yet</p>
                    <p class="empty-state-sub">Tap "Add" to record your first expense!</p>
                </div>
            `;
            return;
        }

        list.innerHTML = expenses.map(e => `
            <div class="expense-item" data-id="${e.id}">
                <div class="expense-icon" style="background: ${e.category_color}22">
                    ${e.category_icon || '💰'}
                </div>
                <div class="expense-details">
                    <div class="expense-category">${e.category_name || 'Uncategorized'}</div>
                    <div class="expense-note">${e.note || '—'}</div>
                </div>
                <div class="expense-meta">
                    <div class="expense-amount">-${formatCurrency(e.amount)}</div>
                    <div class="expense-date">${formatDate(e.date)}</div>
                </div>
                <div class="expense-actions">
                    <button class="edit-btn" onclick="event.stopPropagation(); location.hash='edit/${e.id}'" title="Edit">✏️</button>
                    <button class="delete-btn" onclick="event.stopPropagation(); deleteExpense(${e.id})" title="Delete">🗑️</button>
                </div>
            </div>
        `).join('');
    }

    /* ------------------------------------------
       ADD / EDIT EXPENSE VIEW
       ------------------------------------------ */
    async function loadAddView() {
        // Populate category dropdown — try API, fallback to defaults
        if (!state.categories.length) {
            try {
                state.categories = await API.getCategories();
            } catch (err) {
                console.warn('Could not fetch categories from API, using defaults:', err);
            }
        }
        // If still empty (API returned [] or failed), use defaults
        if (!state.categories || !state.categories.length) {
            state.categories = DEFAULT_CATEGORIES;
        }

        const select = $('#expense-category');
        select.innerHTML = state.categories.map(c =>
            `<option value="${c.id}">${c.icon} ${c.name}</option>`
        ).join('');

        // Default date to today
        $('#expense-date').value = getTodayStr();

        if (state.editingExpense) {
            // Edit mode
            $('#add-view-title').textContent = 'Edit Expense';
            try {
                const expense = await API.getExpenseById(state.editingExpense);
                $('#expense-id').value = expense.id;
                $('#expense-amount').value = expense.amount;
                $('#expense-category').value = expense.category_id;
                $('#expense-date').value = expense.date;
                $('#expense-note').value = expense.note || '';
                $('#expense-recurring').checked = !!expense.is_recurring;
                if (expense.is_recurring) {
                    $('#frequency-group').style.display = 'block';
                }
                $('#save-expense-btn').innerHTML = '<span class="btn-icon">💾</span> Update Expense';
            } catch (err) {
                showToast('Failed to load expense', 'error');
            }
        } else {
            // Add mode
            $('#add-view-title').textContent = 'Add Expense';
            $('#expense-form').reset();
            $('#expense-id').value = '';
            $('#expense-date').value = getTodayStr();
            $('#frequency-group').style.display = 'none';
            $('#save-expense-btn').innerHTML = '<span class="btn-icon">💾</span> Save Expense';
        }
    }

    async function handleExpenseSubmit(e) {
        e.preventDefault();

        const id = $('#expense-id').value;
        const data = {
            amount: parseFloat($('#expense-amount').value) || 0,
            category_id: parseInt($('#expense-category').value),
            date: $('#expense-date').value,
            note: $('#expense-note').value,
            is_recurring: $('#expense-recurring').checked ? 1 : 0,
            frequency: $('#expense-recurring').checked ? $('#expense-frequency').value : '',
        };

        if (data.amount <= 0) {
            showToast('Please enter a valid amount', 'error');
            return;
        }

        try {
            if (id) {
                await API.updateExpense(id, data);
                showToast('Expense updated!', 'success');
            } else {
                await API.addExpense(data);
                showToast('Expense added!', 'success');
            }

            // Handle receipt upload if file selected
            const fileInput = $('#receipt-upload');
            if (fileInput.files.length > 0) {
                const formData = new FormData();
                formData.append('receipt', fileInput.files[0]);
                try {
                    await fetch('/api/upload', { method: 'POST', body: formData });
                } catch (err) {
                    console.warn('Receipt upload failed:', err);
                }
            }

            state.editingExpense = null;
            location.hash = 'dashboard';
        } catch (err) {
            showToast('Failed to save expense', 'error');
        }
    }

    /* ------------------------------------------
       ANALYTICS VIEW
       ------------------------------------------ */
    let barChart = null;
    let pieChart = null;

    async function loadAnalytics() {
        const month = getMonthStr(state.currentMonth);
        $('#analytics-month-label').textContent = getMonthLabel(state.currentMonth);

        try {
            const report = await API.getMonthlyReport(month);

            // Top 3 categories
            renderTopCategories(report.categories || []);

            // Bar chart - daily spending
            renderBarChart(report.daily || [], state.currentMonth);

            // Pie chart - category breakdown
            renderPieChart(report.categories || []);
        } catch (err) {
            console.error('Analytics load error:', err);
        }
    }

    function renderTopCategories(categories) {
        const container = $('#top-categories');
        const top3 = categories.filter(c => c.total > 0).slice(0, 3);

        if (!top3.length) {
            container.innerHTML = `
                <div class="empty-state" style="padding: 24px">
                    <span class="empty-state-icon">🏆</span>
                    <p class="empty-state-sub">No spending data for this month</p>
                </div>
            `;
            return;
        }

        const medals = ['🥇', '🥈', '🥉'];
        container.innerHTML = top3.map((c, i) => `
            <div class="top-cat-item">
                <div class="top-cat-rank">${medals[i]}</div>
                <div class="top-cat-icon">${c.icon}</div>
                <div class="top-cat-name">${c.name}</div>
                <div class="top-cat-amount">${formatCurrency(c.total)}</div>
            </div>
        `).join('');
    }

    function renderBarChart(dailyData, monthDate) {
        const canvas = $('#analytics-bar-chart');
        const ctx = canvas.getContext('2d');

        if (barChart) barChart.destroy();

        // Generate all days of the month
        const year = monthDate.getFullYear();
        const month = monthDate.getMonth();
        const daysInMonth = new Date(year, month + 1, 0).getDate();

        const labels = [];
        const data = [];
        for (let d = 1; d <= daysInMonth; d++) {
            labels.push(d);
            const dayStr = String(d).padStart(2, '0');
            const match = dailyData.find(dd => dd.day === dayStr);
            data.push(match ? match.total : 0);
        }

        const textColor = getComputedStyle(document.body).getPropertyValue('--text-tertiary').trim();
        const gridColor = getComputedStyle(document.body).getPropertyValue('--border').trim();

        barChart = new Chart(ctx, {
            type: 'bar',
            data: {
                labels,
                datasets: [{
                    label: 'Spending',
                    data,
                    backgroundColor: '#6366F180',
                    borderColor: '#6366F1',
                    borderWidth: 1,
                    borderRadius: 4,
                    borderSkipped: false,
                }]
            },
            options: {
                responsive: true,
                maintainAspectRatio: true,
                plugins: {
                    legend: { display: false },
                    tooltip: {
                        callbacks: {
                            label: ctx => formatCurrency(ctx.raw)
                        }
                    }
                },
                scales: {
                    x: {
                        ticks: { color: textColor, font: { size: 10 } },
                        grid: { display: false },
                    },
                    y: {
                        beginAtZero: true,
                        ticks: {
                            color: textColor,
                            font: { size: 10 },
                            callback: v => '₹' + v
                        },
                        grid: { color: gridColor + '40' },
                    }
                }
            }
        });
    }

    function renderPieChart(categories) {
        const canvas = $('#analytics-pie-chart');
        const ctx = canvas.getContext('2d');

        if (pieChart) pieChart.destroy();

        const hasData = categories.some(c => c.total > 0);
        if (!hasData) {
            pieChart = new Chart(ctx, {
                type: 'pie',
                data: {
                    labels: ['No data'],
                    datasets: [{ data: [1], backgroundColor: ['#E2E8F0'], borderWidth: 0 }]
                },
                options: {
                    responsive: true,
                    maintainAspectRatio: true,
                    plugins: { legend: { display: false } }
                }
            });
            return;
        }

        const filtered = categories.filter(c => c.total > 0);
        pieChart = new Chart(ctx, {
            type: 'pie',
            data: {
                labels: filtered.map(c => `${c.icon} ${c.name}`),
                datasets: [{
                    data: filtered.map(c => c.total),
                    backgroundColor: filtered.map(c => c.color),
                    borderWidth: 2,
                    borderColor: getComputedStyle(document.body).getPropertyValue('--bg-secondary').trim(),
                }]
            },
            options: {
                responsive: true,
                maintainAspectRatio: true,
                plugins: {
                    legend: {
                        position: 'bottom',
                        labels: {
                            padding: 12,
                            usePointStyle: true,
                            font: { family: 'Inter', size: 12 },
                            color: getComputedStyle(document.body).getPropertyValue('--text-secondary').trim(),
                        }
                    },
                    tooltip: {
                        callbacks: {
                            label: ctx => ` ${ctx.label}: ${formatCurrency(ctx.raw)}`
                        }
                    }
                }
            }
        });
    }

    /* ------------------------------------------
       SETTINGS VIEW
       ------------------------------------------ */
    async function loadSettings() {
        try {
            const [budgets, auditLog] = await Promise.all([
                API.getBudgets(),
                API.getAuditLog(),
            ]);

            renderBudgetInputs(budgets);
            renderAuditLog(auditLog);

            // Sync theme toggle
            const isDark = document.body.classList.contains('dark');
            $('#theme-toggle-settings').checked = isDark;
        } catch (err) {
            console.error('Settings load error:', err);
        }
    }

    function renderBudgetInputs(budgets) {
        const container = $('#budget-inputs');
        container.innerHTML = budgets.map(b => {
            const pct = b.monthly_limit > 0 ? Math.min(b.percentage, 100) : 0;
            const barColor = b.status === 'exceeded' ? 'var(--danger)' :
                             b.status === 'warning' ? 'var(--warning)' : b.color;
            return `
                <div class="budget-input-row">
                    <div class="budget-cat-icon" style="background: ${b.color}22">${b.icon}</div>
                    <div class="budget-cat-name">
                        ${b.name}
                        ${b.monthly_limit > 0 ? `<div class="budget-progress"><div class="budget-progress-bar" style="width:${pct}%; background:${barColor}"></div></div>` : ''}
                    </div>
                    <input type="number" class="budget-input-field" data-category-id="${b.category_id}"
                           value="${b.monthly_limit || ''}" placeholder="₹ Limit" min="0" step="100">
                </div>
            `;
        }).join('');

        // Add save button
        const existingBtn = container.querySelector('.budget-save-btn');
        if (!existingBtn) {
            const btn = document.createElement('button');
            btn.className = 'btn btn-primary budget-save-btn';
            btn.innerHTML = '<span class="btn-icon">💾</span> Save All Budgets';
            btn.style.marginTop = '16px';
            btn.style.width = '100%';
            btn.addEventListener('click', saveBudgets);
            container.appendChild(btn);
        }
    }

    async function saveBudgets() {
        const inputs = $$('.budget-input-field');
        const promises = [];

        inputs.forEach(input => {
            const categoryId = parseInt(input.dataset.categoryId);
            const limit = parseFloat(input.value) || 0;
            promises.push(API.setBudget({ category_id: categoryId, monthly_limit: limit }));
        });

        try {
            await Promise.all(promises);
            showToast('Budgets saved!', 'success');
            loadSettings();
        } catch (err) {
            showToast('Failed to save budgets', 'error');
        }
    }

    function renderAuditLog(logs) {
        const container = $('#audit-log-list');
        if (!logs.length) {
            container.innerHTML = `
                <div class="empty-state" style="padding: 24px">
                    <span class="empty-state-icon">📋</span>
                    <p class="empty-state-text">No activity yet</p>
                    <p class="empty-state-sub">Actions will appear here</p>
                </div>
            `;
            return;
        }

        container.innerHTML = logs.map(l => `
            <div class="audit-item">
                <div class="audit-dot"></div>
                <div class="audit-details">
                    <div class="audit-action">${l.action}</div>
                    <div class="audit-desc">${l.details}</div>
                </div>
                <div class="audit-time">${formatAuditTime(l.timestamp)}</div>
            </div>
        `).join('');
    }

    function formatAuditTime(ts) {
        try {
            const d = new Date(ts);
            return d.toLocaleDateString('en-IN', { day: 'numeric', month: 'short' }) + ' ' +
                   d.toLocaleTimeString('en-IN', { hour: '2-digit', minute: '2-digit' });
        } catch {
            return ts;
        }
    }

    /* ------------------------------------------
       DELETE EXPENSE (global function)
       ------------------------------------------ */
    window.deleteExpense = async function (id) {
        if (!confirm('Delete this expense?')) return;
        try {
            await API.deleteExpense(id);
            showToast('Expense deleted', 'success');
            loadDashboard();
        } catch (err) {
            showToast('Failed to delete', 'error');
        }
    };

    /* ------------------------------------------
       CSV EXPORT
       ------------------------------------------ */
    function downloadCSV() {
        window.location.href = '/api/reports/export/csv';
    }

    /* ------------------------------------------
       EVENT LISTENERS
       ------------------------------------------ */
    function setupEventListeners() {
        // Navigation
        $$('.nav-link, .tab-link').forEach(link => {
            link.addEventListener('click', (e) => {
                e.preventDefault();
                const view = link.dataset.view;
                location.hash = view;
            });
        });

        // Hash change
        window.addEventListener('hashchange', handleHashChange);

        // Theme toggles
        $('#theme-toggle-sidebar').addEventListener('click', toggleTheme);
        $('#theme-toggle-settings').addEventListener('change', toggleTheme);

        // Expense form
        $('#expense-form').addEventListener('submit', handleExpenseSubmit);

        // Recurring toggle
        $('#expense-recurring').addEventListener('change', (e) => {
            $('#frequency-group').style.display = e.target.checked ? 'block' : 'none';
        });

        // Cancel button
        $('#cancel-expense-btn').addEventListener('click', () => {
            state.editingExpense = null;
            location.hash = 'dashboard';
        });

        // Receipt file display
        $('#receipt-upload').addEventListener('change', (e) => {
            const fileName = e.target.files[0]?.name || 'Choose file or drag here';
            $('#file-upload-label').innerHTML = `<span>📎</span> ${fileName}`;
        });

        // Analytics month navigation
        $('#prev-month-btn').addEventListener('click', () => {
            state.currentMonth.setMonth(state.currentMonth.getMonth() - 1);
            loadAnalytics();
        });

        $('#next-month-btn').addEventListener('click', () => {
            state.currentMonth.setMonth(state.currentMonth.getMonth() + 1);
            loadAnalytics();
        });

        // CSV export
        $('#export-csv-btn').addEventListener('click', downloadCSV);
    }

    /* ------------------------------------------
       INITIALIZATION
       ------------------------------------------ */
    async function init() {
        initTheme();
        setupEventListeners();

        // Process recurring expenses on load
        try {
            await API.processRecurring();
        } catch (err) {
            console.warn('Recurring processing failed:', err);
        }

        // Load categories — fallback to defaults if API unavailable
        try {
            const cats = await API.getCategories();
            if (cats && cats.length) {
                state.categories = cats;
            } else {
                state.categories = DEFAULT_CATEGORIES;
            }
        } catch (err) {
            console.warn('API unavailable, using default categories');
            state.categories = DEFAULT_CATEGORIES;
        }

        // Navigate to initial view
        handleHashChange();
    }

    // Boot
    if (document.readyState === 'loading') {
        document.addEventListener('DOMContentLoaded', init);
    } else {
        init();
    }

})();
