const defaults = {
  amapKey: '',
  defaultDestination: {
    name: '华东交通大学',
    longitude: 115.868517,
    latitude: 28.742945
  },
  defaultBlindStick: {
    name: '导盲杖',
    longitude: 120.077711,
    latitude: 30.305955
  }
}

let localConfig = {}

try {
  localConfig = require('./config.local')
} catch (error) {
  localConfig = {}
}

module.exports = Object.assign({}, defaults, localConfig)
