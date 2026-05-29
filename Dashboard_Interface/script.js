const ctx = document.getElementById('presencaChart').getContext('2d');
const presencaChart = new Chart(ctx, {
    type: 'line',
    data: {
        labels: [], 
        datasets: [{
            label: 'Pessoas na Sala',
            data: [],
            borderColor: '#FF7B47',
            backgroundColor: 'rgba(255, 123, 71, 0.2)',
            borderWidth: 3,
            pointBackgroundColor: '#FF9B71',
            pointRadius: 5,
            fill: true,
            tension: 0.3 
        }]
    },
    options: {
        responsive: true,
        maintainAspectRatio: false,
        scales: {
            y: {
                beginAtZero: true,
                ticks: { stepSize: 1, color: '#A0A0A0' },
                grid: { color: '#333' }
            },
            x: {
                ticks: { color: '#A0A0A0' },
                grid: { color: '#333' }
            }
        },
        plugins: {
            legend: { labels: { color: '#FFF' } }
        }
    }
});

async function atualizarDados() {
    try {
        const response = await fetch('http://192.168.1.42/api/status');
        const data = await response.json();

        // 1. Atualizar Cards
        document.getElementById('pessoas-count').innerText = data.pessoas;
        
        const statusBadge = document.getElementById('sala-status');
        statusBadge.innerText = data.status_texto;
        
        if(data.pessoas > data.limite) {
            statusBadge.className = 'status-badge lotada';
        } else {
            statusBadge.className = 'status-badge';
            if(data.pessoas === 0) statusBadge.style.background = '#A0A0A0';
            else statusBadge.style.background = '#34C759';
        }

        // 2. Atualizar informações do RFID
        document.getElementById('rfid-nome').innerText = data.ultimo_rfid_nome;
        document.getElementById('rfid-ra').innerText = data.ultimo_rfid_ra;
        
        // AQUI LÊ O HORÁRIO DIRETAMENTE DO ESP32
        document.getElementById('rfid-horario').innerText = data.ultimo_rfid_horario;

        // 3. Atualizar o Gráfico
        let dataset = presencaChart.data.datasets[0].data;
        let lastValue = dataset.length > 0 ? dataset[dataset.length - 1] : -1;

        if (data.pessoas !== lastValue) {
            const agora = new Date();
            const horaFormatada = agora.getHours().toString().padStart(2, '0') + ':' + 
                                  agora.getMinutes().toString().padStart(2, '0') + ':' + 
                                  agora.getSeconds().toString().padStart(2, '0');

            presencaChart.data.labels.push(horaFormatada);
            presencaChart.data.datasets[0].data.push(data.pessoas);

            if (presencaChart.data.labels.length > 15) {
                presencaChart.data.labels.shift();
                presencaChart.data.datasets[0].data.shift();
            }

            presencaChart.update();
        }

    } catch (error) {
        console.error("Erro na comunicação com a API:", error);
    }
}

setInterval(atualizarDados, 1000);
atualizarDados();