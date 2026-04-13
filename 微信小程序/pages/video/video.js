const STORAGE_KEY = 'esp32CamIp'

function normalizeIp(value) {
  let ip = (value || '').trim()
  ip = ip.replace(/^https?:\/\//, '')
  ip = ip.split('/')[0]
  ip = ip.split(':')[0]
  return ip
}

Page({
  data: {
    cameraIpInput: '',
    cameraIp: '',
    streamUrl: '',
    captureUrl: '',
    healthUrl: '',
    statusText: '请输入 ESP32-CAM 的局域网 IP',
    streamNonce: Date.now(),
    canPreview: false
  },

  onLoad(options) {
    const optionIp = normalizeIp((options || {}).ip || '')
    const storedIp = normalizeIp(wx.getStorageSync(STORAGE_KEY) || '')
    const cameraIp = optionIp || storedIp

    this.setData({
      cameraIpInput: cameraIp,
      cameraIp
    })

    if (cameraIp) {
      this.rebuildUrls(cameraIp)
      this.setData({
        statusText: `已加载摄像头 IP：${cameraIp}`,
        canPreview: true
      })
    }
  },

  rebuildUrls(cameraIp) {
    const nonce = Date.now()
    this.setData({
      cameraIp,
      streamNonce: nonce,
      streamUrl: `http://${cameraIp}:81/stream?t=${nonce}`,
      captureUrl: `http://${cameraIp}/capture?t=${nonce}`,
      healthUrl: `http://${cameraIp}/health?t=${nonce}`
    })
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
    this.rebuildUrls(cameraIp)
    this.setData({
      statusText: `已保存摄像头 IP：${cameraIp}`,
      canPreview: true
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

    this.rebuildUrls(this.data.cameraIp)
    this.setData({
      statusText: '已刷新视频流连接'
    })
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
      statusText: '正在检查 ESP32-CAM 服务状态...'
    })

    wx.request({
      url: this.data.healthUrl,
      method: 'GET',
      timeout: 5000,
      success: (res) => {
        if (res.statusCode === 200) {
          this.setData({
            statusText: `连接成功：${this.data.cameraIp}`
          })
        } else {
          this.setData({
            statusText: `服务有响应，但状态码异常：${res.statusCode}`
          })
        }
      },
      fail: (err) => {
        this.setData({
          statusText: `连接失败：${err.errMsg || '未知错误'}`
        })
      }
    })
  },

  previewCapture() {
    if (!this.data.captureUrl) {
      return
    }

    wx.previewImage({
      urls: [this.data.captureUrl]
    })
  }
})
