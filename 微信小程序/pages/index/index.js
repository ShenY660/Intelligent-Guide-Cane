var amapFile = require('../../libs/amap-wx.130');
var appConfig = require('../../config');
var myAmapFun = new amapFile.AMapWX({ key: appConfig.amapKey });

const blindStickRefreshMs = Number(appConfig.blindStickRefreshMs) || 10000;

const defaultDestination = appConfig.defaultDestination || {
  name: '华东交通大学',
  longitude: 115.868517,
  latitude: 28.742945
};

const defaultBlindStick = appConfig.defaultBlindStick || {
  name: '导盲杖',
  longitude: 120.077711,
  latitude: 30.305955
};

function buildBlindStickMarker(longitude, latitude) {
  return {
    id: 2,
    name: defaultBlindStick.name,
    longitude: longitude,
    latitude: latitude,
    iconPath: '../../images/stick.png',
    width: 30,
    height: 30
  };
}

Page({
  data: {
    markers: [],
    latitude: '',
    longitude: '',
    textData: {},
    destination: {
      id: 1,
      name: defaultDestination.name,
      longitude: defaultDestination.longitude,
      latitude: defaultDestination.latitude,
      iconPath: '../../images/des.png',
      width: 35,
      height: 35
    },
    myPosition: {},
    scale: 17,
    polyline: [],
    points: [],
    blind_stick: buildBlindStickMarker(defaultBlindStick.longitude, defaultBlindStick.latitude),
    steps: [],
    navigating: false,
    traffic_lights: 0,
    cnt: 0
  },

  onLoad: function () {
    this.toMyposition();
    this.startBlindStickPolling();
  },

  onShow: function () {
    this.startBlindStickPolling();
  },

  onHide: function () {
    this.stopBlindStickPolling();
  },

  onUnload: function () {
    this.stopBlindStickPolling();
  },

  hasBlindStickServerConfig: function () {
    return !!(appConfig.serverBaseUrl && appConfig.deviceId);
  },

  startBlindStickPolling: function () {
    this.stopBlindStickPolling();
    this.syncBlindStickLocation();

    if (!this.hasBlindStickServerConfig()) {
      return;
    }

    this._blindStickTimer = setInterval(() => {
      this.syncBlindStickLocation();
    }, blindStickRefreshMs);
  },

  stopBlindStickPolling: function () {
    if (this._blindStickTimer) {
      clearInterval(this._blindStickTimer);
      this._blindStickTimer = null;
    }
  },

  syncBlindStickLocation: function () {
    if (!this.hasBlindStickServerConfig()) {
      return;
    }

    wx.request({
      url: `${appConfig.serverBaseUrl}/gps/latest`,
      method: 'GET',
      data: {
        device_id: appConfig.deviceId
      },
      success: (result) => {
        const data = result.data || {};
        if (result.statusCode !== 200 || data.latitude === undefined || data.longitude === undefined) {
          return;
        }

        const blindStick = buildBlindStickMarker(Number(data.longitude), Number(data.latitude));
        const nextData = {
          blind_stick: blindStick,
          markers: (this.data.markers || []).map((marker) => {
            return Number(marker.id) === Number(blindStick.id) ? blindStick : marker;
          })
        };

        if (this.data.destination && Number(this.data.destination.id) === Number(blindStick.id)) {
          nextData.destination = blindStick;
        }

        this.setData(nextData);
      }
    });
  },

  ensureMapKey: function () {
    if (appConfig.amapKey) {
      return true;
    }

    wx.showModal({
      title: '缺少地图 Key',
      content: '请先在微信小程序/config.local.js 中配置 amapKey，再使用地点检索和导航功能。',
      showCancel: false
    });
    return false;
  },

  adjustMapView: function () {
    const points = (this.data.markers || []).map((marker) => ({
      latitude: marker.latitude,
      longitude: marker.longitude
    }));

    if (this.data.myPosition.longitude !== undefined && this.data.myPosition.latitude !== undefined) {
      points.push({
        longitude: this.data.myPosition.longitude,
        latitude: this.data.myPosition.latitude
      });
    }

    this.setData({
      points: points
    });
  },

  reverseGps: function () {
    if (!this.ensureMapKey()) return;

    myAmapFun.getRegeo({
      location: '' + this.data.longitude + ',' + this.data.latitude,
      success: (data) => {
        this.setData({
          textData: {
            name: data[0].name,
            desc: data[0].desc
          }
        });
      },
      fail: function (info) {
        wx.showModal({ title: info.errMsg });
      }
    });
  },

  makertap: function (e) {
    var that = this;
    const id = e.detail.markerId;

    if (id !== undefined && id !== null) {
      const markerIndex = that.data.markers.findIndex((marker) => Number(marker.id) === Number(id));
      if (markerIndex >= 0) {
        const mark = Object.assign({}, that.data.markers[markerIndex]);
        if (that.data.navigating) {
          mark.iconPath = '../../images/point.png';
          that.setData({
            markers: that.data.markers.map((item) => Number(item.id) === Number(id) ? mark : item)
          });
        } else {
          mark.iconPath = '../../images/des.png';
          that.setData({
            markers: that.data.markers.map((item) => Number(item.id) === Number(id) ? mark : item),
            textData: {
              name: mark.name,
              desc: ''
            },
            destination: mark
          });
        }
        return;
      }
    }

    var mark = {
      id: that.data.cnt++,
      longitude: e.detail.longitude,
      latitude: e.detail.latitude,
      iconPath: '../../images/des.png',
      width: 30,
      height: 30
    };

    if (that.data.navigating) {
      mark.iconPath = '../../images/point.png';
      that.setData({
        markers: [that.data.destination, mark]
      });
      return;
    }

    if (!that.ensureMapKey()) return;

    myAmapFun.getRegeo({
      location: '' + mark.longitude + ',' + mark.latitude,
      success: function (data) {
        that.setData({
          textData: {
            name: data[0].name,
            desc: data[0].desc
          },
          markers: [mark],
          destination: mark
        });
      },
      fail: function (info) {
        wx.showModal({ title: info.errMsg });
      }
    });
  },

  to_blind: function () {
    const blindStick = this.data.blind_stick || buildBlindStickMarker(defaultBlindStick.longitude, defaultBlindStick.latitude);
    this.setData({
      blind_stick: blindStick,
      markers: [blindStick],
      destination: blindStick
    });
  },

  toMyposition: function () {
    wx.getLocation({
      type: 'gcj02',
      success: (res) => {
        const pos = {
          latitude: res.latitude,
          longitude: res.longitude,
          id: 0,
          name: '我的位置'
        };
        this.setData({
          latitude: res.latitude,
          longitude: res.longitude,
          markers: [],
          myPosition: pos,
          points: [{
            longitude: res.longitude,
            latitude: res.latitude
          }]
        });
        this.reverseGps();
      },
      fail: function (info) {
        wx.showModal({ title: info.errMsg });
      }
    });
  },

  inputAddress: function (e) {
    if (!this.ensureMapKey()) return;

    var that = this;
    var value = e.detail.value;
    if (value === '') return;

    myAmapFun.getInputtips({
      keywords: value,
      success: function (data) {
        if (data.tips.length > 0) {
          var list = data.tips.map((item, index) => {
            return {
              id: index,
              name: item.district + item.name,
              longitude: item.location.split(',')[0],
              latitude: item.location.split(',')[1],
              iconPath: '../../images/point.png',
              joinCluster: true,
              width: 30,
              height: 30
            };
          });
          that.setData({
            markers: list
          });
        }
        that.adjustMapView();
      },
      fail: function (info) {
        wx.showModal({ title: info.errMsg });
      }
    });
  },

  navigation: function () {
    if (!this.ensureMapKey()) return;

    if (this.data.navigating) {
      this.setData({
        navigating: false,
        polyline: [],
        textData: {
          name: this.data.myPosition.name,
          desc: ''
        }
      });
      return;
    }

    this.setData({
      navigating: true
    });

    const that = this;
    const myPosition = this.data.myPosition;
    const destination = this.data.destination;

    if (!myPosition.latitude || !myPosition.longitude || !destination.latitude || !destination.longitude) {
      wx.showModal({ title: '缺少目的地或起始地点信息' });
      return;
    }

    myAmapFun.getWalkingRoute({
      origin: `${myPosition.longitude},${myPosition.latitude}`,
      destination: `${destination.longitude},${destination.latitude}`,
      success: function (data) {
        if (data.paths && data.paths.length > 0) {
          const points = [];
          that.setData({
            steps: data.paths[0].steps,
            traffic_lights: Number(data.paths[0].traffic_lights || 0)
          });

          data.paths[0].steps.forEach((step) => {
            step.polyline.split(';').forEach((point) => {
              const parsed = point.split(',');
              points.push({
                longitude: parseFloat(parsed[0]),
                latitude: parseFloat(parsed[1])
              });
            });
          });

          that.setData({
            polyline: [{ points, color: '#46adf9', width: 6, opacity: 0.8 }],
            markers: [that.data.destination]
          });
          that.adjustMapView();
          that.showNavigation();
        } else {
          wx.showModal({ title: '未找到合适的步行路线' });
        }
      },
      fail: function (info) {
        wx.showModal({ title: info.errMsg });
      }
    });
  },

  toSelected: function (e) {
    var position = e._relatedInfo.anchorTargetText;
    const mark = this.data.markers.slice();

    for (let i = 0; i < mark.length; i++) {
      mark[i].iconPath = '../../images/point.png';
      if (mark[i].name === position) {
        mark[i].iconPath = '../../images/des.png';
        mark[i].width = 40;
        mark[i].height = 40;
        this.setData({
          latitude: mark[i].latitude,
          longitude: mark[i].longitude,
          scale: 17,
          textData: {
            name: mark[i].name,
            desc: mark[i].desc
          },
          destination: {
            id: 1,
            name: mark[i].name,
            longitude: mark[i].longitude,
            latitude: mark[i].latitude,
            iconPath: '../../images/des.png',
            width: 35,
            height: 35
          }
        });
      }
    }

    this.setData({
      markers: mark
    });
  },

  showNavigation: function () {
    let totalTime = 0;
    let totalDistance = 0;
    const steps = this.data.steps;

    steps.forEach((step) => {
      totalDistance += Number(step.distance);
      totalTime += Number(step.duration);
    });

    let distanceStr;
    if (totalDistance > 1000) {
      distanceStr = (totalDistance / 1000).toFixed(2) + 'km';
    } else {
      distanceStr = totalDistance + 'm';
    }

    let hours = Math.floor(totalTime / 3600);
    let minutes = Math.floor((totalTime % 3600) / 60);
    let seconds = totalTime % 60;
    let timeStr = '';

    if (hours > 0) {
      timeStr += hours + ' 小时';
    }
    if (minutes > 0) {
      timeStr += minutes + ' 分钟';
    }
    if (seconds > 0) {
      timeStr += seconds + ' 秒';
    }

    if (!timeStr) {
      timeStr = '0 秒';
    }

    const info = '距离目的地共 ' + distanceStr + '，预计用时 ' + timeStr;
    const lights = this.data.traffic_lights > 0
      ? `沿途共有 ${this.data.traffic_lights} 个红绿灯`
      : '红绿灯信息暂不可用';

    this.setData({
      textData: {
        name: info,
        desc: lights
      }
    });
  }
});
