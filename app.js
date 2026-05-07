let ncwphReady = false;
let ncwph = null;

// WASM module initialize - Module object from ncwph_engine.js
Module.onRuntimeInitialized = () => {
    ncwph = Module;
    ncwphReady = true;
    const statusEl = document.getElementById('engineStatus');
    statusEl.textContent = '✅ Engine loaded';
    statusEl.className = 'status success';
    startCamera();
};

Module.onAbort = (msg) => {
    const statusEl = document.getElementById('engineStatus');
    statusEl.textContent = '❌ Engine failed: ' + msg;
    statusEl.className = 'status error';
};

async function startCamera() {
    try {
        const stream = await navigator.mediaDevices.getUserMedia({ 
            video: { facingMode: 'user', width: 640, height: 480 } 
        });
        const video = document.getElementById('video');
        video.srcObject = stream;
        video.onloadedmetadata = () => {
            document.getElementById('captureBtn').disabled = false;
        };
    } catch(e) {
        console.error('Camera error:', e);
        document.getElementById('captureBtn').textContent = 'Camera unavailable';
    }
}

document.getElementById('captureBtn').addEventListener('click', () => {
    if (!ncwphReady) {
        alert('Engine not loaded yet!');
        return;
    }
    const video = document.getElementById('video');
    const canvas = document.getElementById('canvas');
    canvas.width = video.videoWidth;
    canvas.height = video.videoHeight;
    const ctx = canvas.getContext('2d');
    ctx.drawImage(video, 0, 0);
    const imageData = ctx.getImageData(0, 0, canvas.width, canvas.height);
    
    try {
        const hash = ncwph.computeHash(imageData.data, canvas.width, canvas.height);
        const hashEl = document.getElementById('hashResult');
        hashEl.style.display = 'block';
        hashEl.textContent = hash;
        navigator.clipboard.writeText(hash).then(() => {
            hashEl.textContent += ' (copied!)';
        }).catch(()=>{});
    } catch(e) {
        alert('Hash computation failed: ' + e.message);
    }
});

document.getElementById('compareBtn').addEventListener('click', () => {
    if (!ncwphReady) {
        alert('Engine not loaded yet!');
        return;
    }
    const h1 = document.getElementById('hash1').value.trim();
    const h2 = document.getElementById('hash2').value.trim();
    if (!h1 || !h2) {
        alert('Please enter both hashes');
        return;
    }
    try {
        const result = ncwph.compare(h1, h2);
        const resultEl = document.getElementById('compareResult');
        resultEl.innerHTML = 
            'Similarity: <b>' + (result.similarity * 100).toFixed(1) + '%</b><br>' +
            'Match: <b>' + (result.match ? '✅ YES' : '❌ NO') + '</b>';
    } catch(e) {
        alert('Comparison failed: ' + e.message);
    }
});
