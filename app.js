// app.js – NCWPH client‑side logic
let ncwphModule = null;
let videoReady = false;

// Load WASM module
(async function init() {
  try {
    ncwphModule = await Module(); // from wasm wrapper
  } catch(e) {
    console.error("WASM load failed:", e);
    alert("Failed to load NCWPH engine.");
    return;
  }
  console.log("NCWPH Core loaded");
  startCamera();
})();

async function startCamera() {
  try {
    const stream = await navigator.mediaDevices.getUserMedia({ video: { facingMode: 'user', width: 640, height: 480 } });
    const video = document.getElementById('video');
    video.srcObject = stream;
    video.onloadedmetadata = () => {
      videoReady = true;
      document.getElementById('capture').disabled = false;
    };
  } catch(e) {
    console.error("Camera error:", e);
    document.getElementById('capture').innerText = "Camera unavailable";
  }
}

function captureAndHash() {
  if (!videoReady || !ncwphModule) return;
  const video = document.getElementById('video');
  const canvas = document.getElementById('canvas');
  canvas.width = video.videoWidth;
  canvas.height = video.videoHeight;
  const ctx = canvas.getContext('2d');
  ctx.drawImage(video, 0, 0);
  const imageData = ctx.getImageData(0, 0, canvas.width, canvas.height);
  // Pass to WASM
  const hash = ncwphModule.computeHash(imageData.data, canvas.width, canvas.height);
  document.getElementById('hash-display').innerText = hash;
  // Copy to clipboard
  navigator.clipboard.writeText(hash).catch(()=>{});
}

document.getElementById('capture').addEventListener('click', captureAndHash);

document.getElementById('compare-btn').addEventListener('click', () => {
  const h1 = document.getElementById('hash1').value.trim();
  const h2 = document.getElementById('hash2').value.trim();
  if (!h1 || !h2 || !ncwphModule) return;
  const result = ncwphModule.compare(h1, h2);
  document.getElementById('compare-result').innerHTML = 
    `Similarity: ${result.similarity.toFixed(3)}<br>Match: ${result.match ? '✔️ Yes' : '❌ No'}`;
});
