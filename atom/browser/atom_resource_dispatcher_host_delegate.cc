// Copyright (c) 2015 GitHub, Inc.
// Use of this source code is governed by the MIT license that can be
// found in the LICENSE file.

#include <memory>

#include "atom/browser/atom_resource_dispatcher_host_delegate.h"

#include "atom/browser/login_handler.h"
#include "atom/browser/web_contents_permission_helper.h"
#include "atom/common/platform_util.h"
#include "base/strings/utf_string_conversions.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_frame_host.h"
#include "net/base/escape.h"
#include "net/ssl/client_cert_store.h"
#include "url/gurl.h"

#if defined(USE_NSS_CERTS)
#include "net/ssl/client_cert_store_nss.h"
#elif defined(OS_WIN)
#include "net/ssl/client_cert_store_win.h"
#elif defined(OS_MACOSX)
#include "net/ssl/client_cert_store_mac.h"
#endif

using content::BrowserThread;

namespace atom {

namespace {

void OnOpenExternal(const GURL& escaped_url,
                    bool allowed) {
  if (allowed)
    platform_util::OpenExternal(
#if defined(OS_WIN)
        base::UTF8ToUTF16(escaped_url.spec()),
#else
        escaped_url,
#endif
        true);
}

void HandleExternalProtocolInUI(
    const GURL& url,
    int frame_tree_node_id,
    content::RenderFrameHost* rfh,
    bool has_user_gesture) {
  content::WebContents* web_contents =
    content::WebContents::FromFrameTreeNodeId(frame_tree_node_id);

  if (!web_contents) {
    web_contents = content::WebContents::FromRenderFrameHost(rfh);
  }

  auto permission_helper =
      WebContentsPermissionHelper::FromWebContents(web_contents);
  if (!permission_helper)
    return;

  GURL escaped_url(net::EscapeExternalHandlerValue(url.spec()));
  auto callback = base::Bind(&OnOpenExternal, escaped_url);
  permission_helper->RequestOpenExternalPermission(url, has_user_gesture,
    callback);
}

}  // namespace

AtomResourceDispatcherHostDelegate::AtomResourceDispatcherHostDelegate() {
}

bool AtomResourceDispatcherHostDelegate::HandleExternalProtocol(
    const GURL& url,
    content::ResourceRequestInfo* info) {

  auto render_frame_host =
    content::RenderFrameHost::FromID(info->GetChildID(),
      info->GetRenderFrameID());

  BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
                          base::Bind(&HandleExternalProtocolInUI,
                                     url,
                                     info->GetFrameTreeNodeId(),
                                     render_frame_host,
                                     info->HasUserGesture()));
  return true;
}

content::ResourceDispatcherHostLoginDelegate*
AtomResourceDispatcherHostDelegate::CreateLoginDelegate(
    net::AuthChallengeInfo* auth_info,
    net::URLRequest* request) {
  return new LoginHandler(auth_info, request);
}

std::unique_ptr<net::ClientCertStore>
AtomResourceDispatcherHostDelegate::CreateClientCertStore(
    content::ResourceContext* resource_context) {
  #if defined(USE_NSS_CERTS)
    return std::unique_ptr<net::ClientCertStore>(new net::ClientCertStoreNSS(
        net::ClientCertStoreNSS::PasswordDelegateFactory()));
  #elif defined(OS_WIN)
    return std::unique_ptr<net::ClientCertStore>(new net::ClientCertStoreWin());
  #elif defined(OS_MACOSX)
    return std::unique_ptr<net::ClientCertStore>(new net::ClientCertStoreMac());
  #elif defined(USE_OPENSSL)
    return std::unique_ptr<net::ClientCertStore>();
  #endif
}

}  // namespace atom
