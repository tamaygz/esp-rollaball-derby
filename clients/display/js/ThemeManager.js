'use strict';

/**
 * ThemeManager — loads theme configuration and provides asset references.
 *
 * Reads theme.json from /assets/themes/{id}/ and exposes:
 *   ThemeManager.load(themeId)          → Promise<themeData>
 *   ThemeManager.getPlayerColor(index)  → PIXI hex number (e.g. 0xE53E3E)
 *   ThemeManager.getSpriteAspect()      → width/height ratio parsed from spriteViewBox
 *   ThemeManager.getTexture(key)        → cached PIXI.Texture ('sprite'|'trackBg'|'finishFlag')
 *   ThemeManager.spriteUrl              → '/assets/themes/{id}/sprite.svg'
 *   ThemeManager.trackBgUrl             → '/assets/themes/{id}/track-bg.svg'
 *   ThemeManager.finishFlagUrl          → '/assets/themes/{id}/finish-flag.svg'
 *   ThemeManager.palette                → palette object from theme.json
 *   ThemeManager.id                     → active theme id string
 */
var ThemeManager = (function () {
  var _theme    = null;
  var _themeId  = null;
  var _textures = {};   // themeId → { sprite, trackBg, finishFlag }

  function load(themeId) {
    if (_themeId === themeId && _theme) {
      return Promise.resolve(_theme);
    }
    return fetch('/assets/themes/' + themeId + '/theme.json')
      .then(function (r) {
        if (!r.ok) throw new Error('theme.json fetch failed: ' + r.status);
        return r.json();
      })
      .then(function (data) {
        _theme   = data;
        _themeId = themeId;

        var base          = '/assets/themes/' + themeId + '/';
        var spriteUrl     = base + data.assets.sprite;
        var trackBgUrl    = base + data.assets.trackBg;
        var finishFlagUrl = base + data.assets.finishFlag;

        // Preload all three SVG textures via PIXI Assets so Lane._build() can run synchronously
        return Promise.all([
          PIXI.Assets.load(spriteUrl),
          PIXI.Assets.load(trackBgUrl),
          PIXI.Assets.load(finishFlagUrl),
        ]).then(function (textures) {
          _textures[themeId] = {
            sprite:     textures[0],
            trackBg:    textures[1],
            finishFlag: textures[2],
          };
          return data;
        });
      });
  }

  function getPlayerColor(index) {
    if (!_theme) return 0xffffff;
    var colors = _theme.playerColors;
    var entry  = colors[index % colors.length];
    // entry.pixi is a string like "0xE53E3E"
    return parseInt(entry.pixi, 16);
  }

  function getSpriteAspect() {
    if (!_theme || !_theme.spriteViewBox) return 1.38;
    // "0 0 W H"
    var parts = _theme.spriteViewBox.split(' ');
    var w = parseFloat(parts[2]);
    var h = parseFloat(parts[3]);
    return (w && h) ? w / h : 1.38;
  }

  /**
   * Returns the preloaded PIXI.Texture for the given asset key.
   * @param {'sprite'|'trackBg'|'finishFlag'} key
   * @returns {PIXI.Texture}
   */
  function getTexture(key) {
    if (!_themeId || !_textures[_themeId]) return PIXI.Texture.WHITE;
    return _textures[_themeId][key] || PIXI.Texture.WHITE;
  }

  function _assetUrl(key) {
    if (!_theme || !_themeId) return '';
    return '/assets/themes/' + _themeId + '/' + _theme.assets[key];
  }

  return {
    load:             load,
    getPlayerColor:   getPlayerColor,
    getSpriteAspect:  getSpriteAspect,
    getTexture:       getTexture,
    get id()           { return _themeId; },
    get spriteUrl()    { return _assetUrl('sprite'); },
    get trackBgUrl()   { return _assetUrl('trackBg'); },
    get finishFlagUrl(){ return _assetUrl('finishFlag'); },
    get palette()      { return _theme ? _theme.palette : {}; },
  };
}());
