const addon = require('bindings')('addon');
const account = require('./account.json');
const toNumber = require('./to.json');

addon.SetAccountInfo(account.fromURI,
                     account.reqURI,
	             account.userid,
	             account.password);

addon.SetOnSMSReceived(function(msg){
  console.log(msg);
});

const messages = new Array();

const sender = setInterval(() => {
  if (messages.length !== 0) {
    addon.SendSMS(toNumber.to, messages.shift());
  }
}, 2000);

messages.push("Message 1");
messages.push("Message 2");
messages.push("Message 3");
messages.push("Message 4");

setTimeout(() => {
  clearInterval(sender);
  addon.Stop();
}, 60000);
