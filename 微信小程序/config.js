const defaults = {
  amapKey: '',
  serverBaseUrl: '',
  deviceId: '',
  blindStickRefreshMs: 10000,
  defaultDestination: {
    name: 'Example Destination',
    longitude: 116.397428,
    latitude: 39.90923
  },
  defaultBlindStick: {
    name: 'Guide Cane',
    longitude: 116.397428,
    latitude: 39.90923
  }
}

function isPlaceholder(value) {
  return typeof value === 'string' && /(YOUR_|CHANGE_ME|XXXX)/i.test(value)
}

let localConfig = {}

try {
  localConfig = require('./config.local')
} catch (error) {
  localConfig = {}
}

const mergedConfig = Object.assign({}, defaults, localConfig)

if (isPlaceholder(mergedConfig.amapKey)) {
  mergedConfig.amapKey = ''
}

if (isPlaceholder(mergedConfig.serverBaseUrl)) {
  mergedConfig.serverBaseUrl = ''
}

if (isPlaceholder(mergedConfig.deviceId)) {
  mergedConfig.deviceId = ''
}

module.exports = mergedConfig
