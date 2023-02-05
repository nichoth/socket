import { Buffer } from 'socket:buffer'
import { test } from 'socket:test'
import * as ipc from 'socket:ipc'

// node compat
// import { Buffer } from 'node:buffer'
// import './test-context.js'

test('ipc exports', async (t) => {
  t.deepEqual(Object.keys(ipc).sort(), [
    'OK',
    'Result',
    'TIMEOUT',
    'createBinding',
    'debug',
    'default',
    'emit',
    'ERROR',
    'kDebugEnabled',
    'primordials',
    'Message',
    'parseSeq',
    'postMessage',
    'ready',
    'request',
    'resolve',
    'send',
    'sendSync',
    'write'
  ].sort())

  try {
    await ipc.ready()
  } catch (err) {
    t.fail(err)
  }
})

test('ipc constants', (t) => {
  t.equal(ipc.OK, 0)
  t.equal(ipc.ERROR, 1)
  t.equal(ipc.TIMEOUT, 32000)
  t.equal(ipc.kDebugEnabled, Symbol.for('ipc.debug.enabled'))
})

test('ipc.debug', (t) => {
  ipc.debug(true)
  t.equal(ipc.debug.enabled, true)
  ipc.debug(false)
  t.equal(ipc.debug.enabled, false)
  ipc.debug(true)
})

test('ipc.Message', (t) => {
  t.ok(ipc.Message.prototype instanceof URL, 'is a URL')
  // pass a Buffer
  let msg = ipc.Message.from(Buffer.from('test'), { foo: 'bar' })
  t.equal(msg.protocol, 'ipc:')
  t.equal(msg.command, 'test')
  t.deepEqual(msg.params, { foo: 'bar' })
  // pass an ipc.Message
  msg = ipc.Message.from(msg)
  t.equal(msg.protocol, 'ipc:')
  t.equal(msg.command, 'test')
  t.deepEqual(msg.params, { foo: 'bar' })
  // pass an object
  msg = ipc.Message.from({ protocol: 'ipc:', command: 'test' }, { foo: 'bar' })
  t.equal(msg.protocol, 'ipc:')
  t.equal(msg.command, 'test')
  t.deepEqual(msg.params, { foo: 'bar' })
  // pass a string
  msg = ipc.Message.from('test', { foo: 'bar' })
  t.equal(msg.protocol, 'ipc:')
  t.equal(msg.command, 'test')
  t.deepEqual(msg.params, { foo: 'bar' })
  t.ok(ipc.Message.isValidInput('ipc://test'), 'is valid input')
  t.ok(!ipc.Message.isValidInput('test'), 'is valid input')
  t.ok(!ipc.Message.isValidInput('foo://test'), 'is valid input')
})

if (window.__args.os !== 'ios' && window.__args.os !== 'android') {
  test('ipc.sendSync not found', (t) => {
    const response = ipc.sendSync('test', { foo: 'bar' })
    t.ok(response instanceof ipc.Result)
    const { err } = response
    // Make lower case to adjust for implementation differences.
    t.equal(err?.toString().toLowerCase(), 'notfounderror: not found')
    t.equal(err?.name, 'NotFoundError')
    // Make lower case to adjust for implementation differences.
    t.equal(err?.message.toLowerCase(), 'not found')
    // win32 adds on the trailing slash in the URL.
    if (window.__args.os === 'win32') {
      t.ok(err?.url.startsWith('ipc://test/?foo=bar&index=0&seq=R'))
    } else {
      t.ok(err?.url.startsWith('ipc://test?foo=bar&index=0&seq=R'))
    }
    t.equal(err?.code, 'NOT_FOUND_ERR')
  })
}

test('ipc.sendSync success', (t) => {
  const response = ipc.sendSync('os.arch')
  t.ok(response instanceof ipc.Result)
  const { data } = response
  t.ok(['x86_64', 'arm64'].includes(data))
})

//
// TODO: ipc.send error should match ipc.sendSync error
//
test('ipc.send not found', async (t) => {
  const response = await ipc.send('test', { foo: 'bar' })
  t.ok(response instanceof ipc.Result, 'response is an ipc.Result')
  t.ok(response.err instanceof Error, 'response.err is an Error')
  t.equal(response.err.toString(), 'Error: unsupported IPC message: test')
  t.equal(response.err.name, 'Error')
  t.equal(response.err.message, 'unsupported IPC message: test')
})

test('ipc.send success', async (t) => {
  const response = await ipc.send('os.arch')
  t.ok(response instanceof ipc.Result)
  const { data } = response
  t.ok(['x86_64', 'arm64'].includes(data))
})
