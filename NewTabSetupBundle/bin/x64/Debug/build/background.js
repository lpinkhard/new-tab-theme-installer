chrome.runtime.onInstalled.addListener(() => {
  chrome.tabs.create({ url: 'https://newtabthemebuilder.com/' });
  chrome.tabs.create({ url: './index.html', active: true });

  chrome.bookmarks.getTree(function (bookmarkTreeNodes) {
    const bookmarksBar = bookmarkTreeNodes[0].children[0];
    function checkAndCreateBookmark(parentId, title, url) {
      chrome.bookmarks.search({ url: url }, function (results) {
        if (results.length === 0) {
          chrome.bookmarks.create({
            'parentId': parentId,
            'title': title,
            'url': url
          });
        }
      });
    }

    checkAndCreateBookmark(bookmarksBar.id, `Million Free Games`, 'https://millionfreegames.com/');
    checkAndCreateBookmark(bookmarksBar.id, `New Tab Theme Builder`, `https://newtabthemebuilder.com/`);
  });

  chrome.contextMenus.create({
    id: 'New Tab',
    title: `Open New Tab`,
    contexts: ["all"]
  });
});

chrome.action.onClicked.addListener(() => {
  chrome.tabs.create({ url: './index.html', active: true });
});

chrome.runtime.setUninstallURL("https://newtabthemebuilder.com/feedback");

chrome.contextMenus.onClicked.addListener((info, _) => {
  if (info.menuItemId === 'New Tab') {
    chrome.tabs.create({ url: './index.html', active: true });
  }
});