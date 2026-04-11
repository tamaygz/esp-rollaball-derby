'use strict';

const os = require('os');
const { Router } = require('express');

const router = Router();

router.get('/', (req, res) => {
  const hostname = os.hostname();
  res.json({
    status: 'ok',
    uptime: process.uptime(),
    hostname,
    mdns: {
      enabled: true,
      serviceType: '_derby._tcp',
      serviceName: 'derby-server',
      hostname: `${hostname}.local`,
    },
  });
});

module.exports = router;
