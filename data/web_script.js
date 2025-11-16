//**代码作者：搞硬件的辛工，开源地址：https://github.com/xzj2004/ircam 
//**项目合作微信：mcugogogo，如果您有任何创意或想法或者项目落地需求都可以和我联系 
//**本项目开源协议MIT，允许自由修改发布及商用 
//**本项目仅用于学习交流，请勿用于非法用途 

const canvas = document.getElementById('thermalCanvas');
canvas.style.imageRendering = 'pixelated';
const ctx = canvas.getContext('2d');
ctx.imageSmoothingEnabled = false;

const srcWidth = 32, srcHeight = 24;
let dstWidth = 96, dstHeight = 72;
let streaming = false, frameCount = 0, lastTime = Date.now();

// 缓存DOM元素
const minTempElement = document.getElementById('minTemp');
const maxTempElement = document.getElementById('maxTemp');
const colormapSelect = document.getElementById('colormap');
const interpolationSelect = document.getElementById('interpolation');

// 预计算colormap查找表
const colormapLUT = new Map();
const lutSize = 256;

const colormaps = {
    hot: [[0,0,0],[0.3,0,0],[0.6,0.3,0],[0.9,0.6,0],[1,0.9,0],[1,1,0.3],[1,1,0.6],[1,1,1]],
    jet: [[0,0,0.5],[0,0,1],[0,1,1],[1,1,0],[1,0,0],[0.5,0,0]],
    inferno: [[0,0,0],[0.2,0.1,0.3],[0.4,0,0.4],[0.6,0,0.5],[0.8,0.3,0.3],[1,0.6,0],[1,0.9,0.2]],
    turbo: [[0.18995,0.07176,0.23217],[0.25107,0.37408,0.77204],[0.19802,0.72896,0.90123],[0.83327,0.86547,0.24252],[0.99942,0.62738,0.09517]]
};

let lastData = null;
let minTemp = 0, maxTemp = 50;
let tempRangeSmoothed = false;
let prevMinTemp = 0, prevMaxTemp = 50;
let manualTempRange = false;
let manualMinTemp = 15, manualMaxTemp = 35;
let pendingRequest = false;
let lastRequestTime = 0;
const minRequestInterval = 30; // 减少请求间隔到30ms

// 添加最高温度位置标记变量
let hotspotX = 0, hotspotY = 0;
let hotspotTemp = 0;

// 预计算colormap查找表
function generateColormapLUT(colormap) {
    const lut = new Uint32Array(lutSize);
    const colors = colormaps[colormap];
    const alphaChannel = 255 << 24;
    
    for (let i = 0; i < lutSize; i++) {
        const t = i / (lutSize - 1);
        const color = interpolateColor(colors, t);
        const r = Math.min(255, Math.max(0, Math.round(color[0] * 255)));
        const g = Math.min(255, Math.max(0, Math.round(color[1] * 255)));
        const b = Math.min(255, Math.max(0, Math.round(color[2] * 255)));
        lut[i] = alphaChannel | (b << 16) | (g << 8) | r;
    }
    return lut;
}

// 初始化所有colormap的LUT
function initColormapLUTs() {
    for (const name in colormaps) {
        colormapLUT.set(name, generateColormapLUT(name));
    }
}

function nearestNeighbor(temps, x, y) {
    const ix = Math.floor(x);
    const iy = Math.floor(y);
    return temps[iy * srcWidth + ix];
}

function bilinearInterpolate(temps, x, y) {
    const x1 = Math.floor(x);
    const y1 = Math.floor(y);
    const x2 = Math.min(x1 + 1, srcWidth - 1);
    const y2 = Math.min(y1 + 1, srcHeight - 1);
    const fx = x - x1;
    const fy = y - y1;
    const a = temps[y1 * srcWidth + x1];
    const b = temps[y1 * srcWidth + x2];
    const c = temps[y2 * srcWidth + x1];
    const d = temps[y2 * srcWidth + x2];
    const top = a + (b - a) * fx;
    const bottom = c + (d - c) * fx;
    return top + (bottom - top) * fy;
}

function interpolateColor(colors, t) {
    if (t <= 0) return colors[0];
    if (t >= 1) return colors[colors.length - 1];
    const idx = t * (colors.length - 1);
    const i = Math.floor(idx);
    const f = idx - i;
    if (!colors[i] || !colors[i + 1]) return colors[0];
    const c1 = colors[i];
    const c2 = colors[i + 1];
    return [
        c1[0] + (c2[0] - c1[0]) * f,
        c1[1] + (c2[1] - c1[1]) * f,
        c1[2] + (c2[2] - c1[2]) * f
    ];
}

function getColor(value, min, max) {
    const ratio = Math.min(1, Math.max(0, (value - min) / (max - min)));
    const colormap = colormaps[document.getElementById('colormap').value] || colormaps.hot;
    const color = interpolateColor(colormap, ratio);
    const r = Math.min(255, Math.max(0, Math.round(color[0] * 255)));
    const g = Math.min(255, Math.max(0, Math.round(color[1] * 255)));
    const b = Math.min(255, Math.max(0, Math.round(color[2] * 255)));
    return `rgb(${r},${g},${b})`;
}

function updateColormap() {
    if (lastData) updateThermalImage(lastData);
}

function changeResolution() {
    const scaleOption = document.getElementById('resolution').value;
    if (scaleOption === 'fullscreen') {
        const viewportWidth = window.innerWidth;
        const viewportHeight = window.innerHeight - 120;
        const aspectRatio = srcWidth / srcHeight;
        if (viewportWidth / viewportHeight > aspectRatio) {
            dstHeight = Math.floor(viewportHeight);
            dstWidth = Math.floor(dstHeight * aspectRatio);
        } else {
            dstWidth = Math.floor(viewportWidth);
            dstHeight = Math.floor(dstWidth / aspectRatio);
        }
        canvas.style.width = dstWidth + 'px';
        canvas.style.height = dstHeight + 'px';
    } else {
        const scale = parseInt(scaleOption);
        dstWidth = srcWidth * scale;
        dstHeight = srcHeight * scale;
        canvas.style.width = 'auto';
        canvas.style.height = 'auto';
    }
    canvas.width = dstWidth;
    canvas.height = dstHeight;
    if (lastData) updateThermalImage(lastData);
}

// 修改toggleAdvanced函数
function toggleAdvanced() {
    const advControls = document.getElementById('advancedControls');
    const isVisible = advControls.style.display !== 'none';
    advControls.style.display = isVisible ? 'none' : 'block';
    if (currentLang === 'zh') {
        document.getElementById('toggleAdvancedButton').textContent = isVisible ? '高级设置' : '隐藏设置';
    } else {
        document.getElementById('toggleAdvancedButton').textContent = isVisible ? 'Advanced' : 'Hide';
    }
}

function toggleTempSmooth() {
    tempRangeSmoothed = document.getElementById('tempSmooth').checked;
}

function applyManualRange() {
    manualMinTemp = parseFloat(document.getElementById('minTempManual').value);
    manualMaxTemp = parseFloat(document.getElementById('maxTempManual').value);
    if (manualMinTemp >= manualMaxTemp) {
        alert("Minimum temperature must be less than maximum temperature!");
        return;
    }
    manualTempRange = true;
}

function resetAutoRange() {
    manualTempRange = false;
}

function updateEmissivity() {
    const emissValue = parseFloat(document.getElementById('emissivity').value);
    if (emissValue < 0.1 || emissValue > 1.0) {
        alert("Emissivity must be between 0.1 and 1.0!");
        return;
    }
    fetch('/update-emissivity?value=' + emissValue)
        .then(response => response.text())
        .then(result => console.log('Emissivity updated:', result))
        .catch(error => console.error('Failed to update emissivity:', error));
}

// 添加高斯模糊函数
function gaussianBlur(data, width, height, radius) {
    const kernel = generateGaussianKernel(radius);
    const result = new Float32Array(data.length);
    const kernelSize = radius * 2 + 1;
    
    // 水平方向模糊
    for (let y = 0; y < height; y++) {
        for (let x = 0; x < width; x++) {
            let sum = 0;
            let weightSum = 0;
            
            for (let i = -radius; i <= radius; i++) {
                const px = Math.min(Math.max(x + i, 0), width - 1);
                const weight = kernel[i + radius];
                sum += data[y * width + px] * weight;
                weightSum += weight;
            }
            
            result[y * width + x] = sum / weightSum;
        }
    }
    
    // 垂直方向模糊
    const temp = new Float32Array(data.length);
    for (let x = 0; x < width; x++) {
        for (let y = 0; y < height; y++) {
            let sum = 0;
            let weightSum = 0;
            
            for (let i = -radius; i <= radius; i++) {
                const py = Math.min(Math.max(y + i, 0), height - 1);
                const weight = kernel[i + radius];
                sum += result[py * width + x] * weight;
                weightSum += weight;
            }
            
            temp[y * width + x] = sum / weightSum;
        }
    }
    
    return temp;
}

// 生成高斯核
function generateGaussianKernel(radius) {
    const size = radius * 2 + 1;
    const kernel = new Float32Array(size);
    const sigma = radius / 2;
    let sum = 0;
    
    for (let i = 0; i < size; i++) {
        const x = i - radius;
        kernel[i] = Math.exp(-(x * x) / (2 * sigma * sigma));
        sum += kernel[i];
    }
    
    // 归一化
    for (let i = 0; i < size; i++) {
        kernel[i] /= sum;
    }
    
    return kernel;
}

// 语言切换功能
let currentLang = 'zh';

function toggleLanguage() {
    currentLang = currentLang === 'zh' ? 'en' : 'zh';
    updateLanguage();
}

function updateLanguage() {
    const elements = document.querySelectorAll('[data-lang-zh]');
    elements.forEach(el => {
        const zhText = el.getAttribute('data-lang-zh');
        const enText = el.getAttribute('data-lang-en');
        el.textContent = currentLang === 'zh' ? zhText : enText;
    });
    
    // 更新高级设置按钮文本
    const advButton = document.getElementById('toggleAdvancedButton');
    const advControls = document.getElementById('advancedControls');
    const isVisible = advControls.style.display !== 'none';
    if (currentLang === 'zh') {
        advButton.textContent = isVisible ? '隐藏设置' : '高级设置';
    } else {
        advButton.textContent = isVisible ? 'Hide' : 'Advanced';
    }
}

// 在DOMContentLoaded事件中更新高级设置HTML
document.addEventListener('DOMContentLoaded', function() {
    document.getElementById('resolution').value = 'fullscreen';
    colormapSelect.value = 'jet';
    interpolationSelect.value = 'bilinear';
    
    // 初始化colormap查找表
    initColormapLUTs();
    
    changeResolution();

    // 添加事件监听器
    colormapSelect.addEventListener('change', updateColormap);
    interpolationSelect.addEventListener('change', updateColormap);
    document.getElementById('resolution').addEventListener('change', changeResolution);
    document.getElementById('startButton').addEventListener('click', startStream);
    document.getElementById('stopButton').addEventListener('click', stopStream);
    document.getElementById('toggleAdvancedButton').addEventListener('click', toggleAdvanced);
    document.getElementById('tempSmooth').addEventListener('change', toggleTempSmooth);
    document.getElementById('applyManualRangeButton').addEventListener('click', applyManualRange);
    document.getElementById('resetAutoRangeButton').addEventListener('click', resetAutoRange);
    document.getElementById('updateEmissivityButton').addEventListener('click', updateEmissivity);

    // 添加高斯模糊滑块事件监听
    const blurSlider = document.getElementById('blurRadius');
    const blurValue = document.getElementById('blurValue');
    blurSlider.addEventListener('input', function() {
        blurValue.textContent = this.value;
        if (lastData) updateThermalImage(lastData);
    });
});

// Add window resize event listener
window.addEventListener('resize', function() {
    if (document.getElementById('resolution').value === 'fullscreen') {
        changeResolution();
    }
});

// Update FPS display
setInterval(function() {
    var currentTime = Date.now();
    var fps = Math.round(frameCount / ((currentTime - lastTime) / 1000));
    document.getElementById('frameRate').textContent = fps;
    frameCount = 0;
    lastTime = currentTime;
}, 1000);

// 使用requestAnimationFrame来优化动画性能
let animationFrameId = null;

function startStream() {
    if (!streaming) {
        streaming = true;
        animationFrameId = requestAnimationFrame(fetchThermalData);
    }
}

function stopStream() {
    streaming = false;
    if (animationFrameId) {
        cancelAnimationFrame(animationFrameId);
        animationFrameId = null;
    }
}

async function fetchThermalData() {
    if (!streaming) return;

    const now = Date.now();
    const elapsed = now - lastRequestTime;

    if (elapsed < minRequestInterval) {
        animationFrameId = requestAnimationFrame(fetchThermalData);
        return;
    }

    try {
        lastRequestTime = now;
        const response = await fetch('/thermal-data');
        const data = await response.json();
        updateThermalImage(data);
    } catch (error) {
        console.error('Error fetching thermal data:', error);
        streaming = false;
    }

    if (streaming) {
        animationFrameId = requestAnimationFrame(fetchThermalData);
    }
}

function updateThermalImage(data) {
    if (!data || !data.length) return;
    lastData = data;
    const temperatures = new Float32Array(data);
    
    // 应用高斯模糊
    const blurRadius = parseInt(document.getElementById('blurRadius').value);
    const processedTemps = blurRadius > 0 ? gaussianBlur(temperatures, srcWidth, srcHeight, blurRadius) : temperatures;

    // 使用更快的方法找到最小和最大温度，同时记录最高温度位置
    let rawMinTemp = processedTemps[0];
    let rawMaxTemp = processedTemps[0];
    let maxTempIndex = 0;
    
    // 使用 for...of 循环和 TypedArray 来提高性能
    let i = 0;
    for (const temp of processedTemps) {
        if (temp < rawMinTemp) rawMinTemp = temp;
        if (temp > rawMaxTemp) {
            rawMaxTemp = temp;
            maxTempIndex = i;
        }
        i++;
    }

    // 计算最高温度点的源坐标（考虑旋转）
    const originalX = maxTempIndex % srcWidth;
    const originalY = Math.floor(maxTempIndex / srcWidth);
    // 旋转180度后的坐标
    hotspotX = srcWidth - 1 - originalX;
    hotspotY = srcHeight - 1 - originalY;
    hotspotTemp = rawMaxTemp;

    if (manualTempRange) {
        minTemp = manualMinTemp;
        maxTemp = manualMaxTemp;
    } else if (tempRangeSmoothed) {
        minTemp = prevMinTemp * 0.8 + rawMinTemp * 0.2;
        maxTemp = prevMaxTemp * 0.2 + rawMaxTemp * 0.8;
        prevMinTemp = minTemp;
        prevMaxTemp = maxTemp;
    } else {
        minTemp = rawMinTemp;
        maxTemp = rawMaxTemp;
    }

    minTempElement.textContent = minTemp.toFixed(1);
    maxTempElement.textContent = maxTemp.toFixed(1);

    if (!window.cachedImageData || window.cachedImageData.width !== dstWidth || window.cachedImageData.height !== dstHeight) {
        window.cachedImageData = ctx.createImageData(dstWidth, dstHeight);
    }
    const imageData = window.cachedImageData;

    const useNearest = interpolationSelect.value === 'nearest';
    const data32 = new Uint32Array(imageData.data.buffer);
    const currentColormap = colormapLUT.get(colormapSelect.value);
    const tempRange = maxTemp - minTemp;

    for (let y = 0; y < dstHeight; y++) {
        const srcY = (y / dstHeight) * (srcHeight - 1);
        for (let x = 0; x < dstWidth; x++) {
            const srcX = (x / dstWidth) * (srcWidth - 1);
            
            // 旋转180度：将源坐标翻转
            const rotatedSrcX = srcWidth - 1 - srcX;
            const rotatedSrcY = srcHeight - 1 - srcY;
            
            const temp = useNearest ? 
                nearestNeighbor(processedTemps, rotatedSrcX, rotatedSrcY) : 
                bilinearInterpolate(processedTemps, rotatedSrcX, rotatedSrcY);
            
            // 使用查找表获取颜色
            const ratio = Math.min(1, Math.max(0, (temp - minTemp) / tempRange));
            const lutIndex = Math.min(lutSize - 1, Math.floor(ratio * (lutSize - 1)));
            data32[y * dstWidth + x] = currentColormap[lutIndex];
        }
    }

    ctx.putImageData(imageData, 0, 0);

    // 绘制最高温度位置标记
    const boxSize = Math.min(dstWidth / 16, dstHeight / 16); // 框的大小随画布缩放
    const markerX = (hotspotX / (srcWidth - 1)) * dstWidth;
    const markerY = (hotspotY / (srcHeight - 1)) * dstHeight;

    // 绘制十字准线
    ctx.beginPath();
    ctx.strokeStyle = 'rgba(255, 255, 255, 0.8)';
    ctx.lineWidth = 2;
    
    // 绘制方框
    ctx.strokeRect(
        markerX - boxSize/2,
        markerY - boxSize/2,
        boxSize,
        boxSize
    );

    // 绘制温度标签
    ctx.font = '14px Arial';
    ctx.fillStyle = 'white';
    ctx.textAlign = 'left';
    ctx.textBaseline = 'top';
    const text = `${hotspotTemp.toFixed(1)}°C`;
    const padding = 5;
    const textX = markerX + boxSize/2 + padding;
    const textY = markerY - boxSize/2;
    
    // 绘制文本背景
    const textMetrics = ctx.measureText(text);
    const textHeight = 14; // 假设字体高度
    ctx.fillStyle = 'rgba(0, 0, 0, 0.5)';
    ctx.fillRect(
        textX - padding,
        textY - padding,
        textMetrics.width + padding * 2,
        textHeight + padding * 2
    );
    
    // 绘制文本
    ctx.fillStyle = 'white';
    ctx.fillText(text, textX, textY);

    frameCount++;
}