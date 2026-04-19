const STORAGE_KEY = 'esp32CamIp'
const PREVIEW_INTERVAL_MS = 800
const PREVIEW_RETRY_MS = 1500

function normalizeIp(value) {
  let ip = (value || '').trim()
  ip = ip.replace(/^https?:\/\//, '')
  ip = ip.split('/')[0]
  ip = ip.split(':')[0]
  return ip
}

Page({
  previewTimer: null,
  isPreviewRunning: false,
  isFrameLoading: false,
  hasLoadedFrame: false,

  data: {
    cameraIpInput: '',
    cameraIp: '',
    previewUrl: '',
    captureUrl: '',
    healthUrl: '',
    statusText: '请输入 ESP32-CAM 的局域网 IP',
    canPreview: false
  },

  onLoad(options) {
    const optionIp = normalizeIp((options || {}).ip || '')
    const storedIp = normalizeIp(wx.getStorageSync(STORAGE_KEY) || '')
    const cameraIp = optionIp || storedIp

    if (!cameraIp) {
      return
    }

    this.setCameraIp(cameraIp, {
      statusText: `已加载摄像头 IP：${cameraIp}`,
      startPreview: true
    })
  },

  onShow() {
    if (this.data.cameraIp) {
      this.startPreview()
    }
  },

  onHide() {
    this.stopPreview()
  },

  onUnload() {
    this.stopPreview()
  },

  buildHealthUrl(cameraIp) {
    return `http://${cameraIp}/health`
  },

  buildCaptureUrl(cameraIp) {
    return `http://${cameraIp}/capture`
  },

  buildPreviewUrl(cameraIp) {
    return `http://${cameraIp}/capture?t=${Date.now()}`
  },

  setCameraIp(cameraIp, options = {}) {
    const statusText = options.statusText || `已保存摄像头 IP：${cameraIp}`
    const startPreview = options.startPreview !== false

    this.hasLoadedFrame = false
    this.setData({
      cameraIpInput: cameraIp,
      cameraIp,
      previewUrl: this.buildPreviewUrl(cameraIp),
      captureUrl: this.buildCaptureUrl(cameraIp),
      healthUrl: this.buildHealthUrl(cameraIp),
      statusText,
      canPreview: true
    })

    if (startPreview) {
      this.startPreview()
    }
  },

  refreshPreviewFrame(statusText = '') {
    if (!this.data.cameraIp || this.isFrameLoading) {
      return
    }

    this.isFrameLoading = true
    const nextState = {
      previewUrl: this.buildPreviewUrl(this.data.cameraIp),
      canPreview: true
    }

    if (statusText) {
      nextState.statusText = statusText
    }

    this.setData(nextState)
  },

  startPreview() {
    if (!this.data.cameraIp) {
      return
    }

    this.stopPreview()
    this.isPreviewRunning = true
    this.scheduleNextFrame(0, '正在刷新抓拍画面...')
  },

  stopPreview() {
    if (this.previewTimer) {
      clearTimeout(this.previewTimer)
      this.previewTimer = null
    }
    this.isPreviewRunning = false
    this.isFrameLoading = false
  },

  scheduleNextFrame(delay = PREVIEW_INTERVAL_MS, statusText = '') {
    if (!this.isPreviewRunning || !this.data.cameraIp) {
      return
    }

    if (this.previewTimer) {
      clearTimeout(this.previewTimer)
    }

    this.previewTimer = setTimeout(() => {
      this.previewTimer = null
      this.refreshPreviewFrame(statusText)
    }, delay)
  },

  onIpInput(e) {
    this.setData({
      cameraIpInput: e.detail.value
    })
  },

  saveCameraIp() {
    const cameraIp = normalizeIp(this.data.cameraIpInput)
    if (!cameraIp) {
      wx.showToast({
        title: '请输入有效 IP',
        icon: 'none'
      })
      return
    }

    wx.setStorageSync(STORAGE_KEY, cameraIp)
    this.setCameraIp(cameraIp, {
      statusText: `已保存摄像头 IP：${cameraIp}`,
      startPreview: true
    })
  },

  refreshStream() {
    if (!this.data.cameraIp) {
      wx.showToast({
        title: '请先保存摄像头 IP',
        icon: 'none'
      })
      return
    }

    this.hasLoadedFrame = false
    this.stopPreview()
    this.isPreviewRunning = true
    this.scheduleNextFrame(0, '正在重新获取画面...')
  },

  testConnection() {
    if (!this.data.cameraIp) {
      wx.showToast({
        title: '请先保存摄像头 IP',
        icon: 'none'
      })
      return
    }

    this.setData({
      statusText: '正在检查 ESP32-CAM 连接...'
    })

    wx.request({
      url: `${this.data.healthUrl}?t=${Date.now()}`,
      method: 'GET',
      timeout: 3000,
      success: (res) => {
        if (res.statusCode === 200) {
          this.setData({
            statusText: `连接成功：${this.data.cameraIp}`
          })
          this.hasLoadedFrame = false
          this.startPreview()
        } else {
          this.setData({
            statusText: `服务已响应，但状态码异常：${res.statusCode}`
          })
        }
      },
      fail: (err) => {
        this.setData({
          statusText: `健康检查失败，改为尝试抓拍：${err.errMsg || '未知错误'}`
        })
        this.hasLoadedFrame = false
        this.startPreview()
      }
    })
  },

  previewCapture() {
    if (!this.data.cameraIp) {
      wx.showToast({
        title: '请先保存摄像头 IP',
        icon: 'none'
      })
      return
    }

    wx.previewImage({
      urls: [this.buildPreviewUrl(this.data.cameraIp)]
    })
  },

  onPreviewLoad() {
    this.isFrameLoading = false

    if (!this.data.cameraIp) {
      return
    }

    if (!this.hasLoadedFrame) {
      this.hasLoadedFrame = true
      this.setData({
        statusText: `画面已连接：${this.data.cameraIp}`
      })
    }

    this.scheduleNextFrame()
  },

  onPreviewError(e) {
    const errMsg = (((e || {}).detail || {}).errMsg) || '图片加载失败'
    this.isFrameLoading = false
    this.setData({
      statusText: `画面加载失败：${errMsg}`
    })

    this.scheduleNextFrame(PREVIEW_RETRY_MS)
  }
})
