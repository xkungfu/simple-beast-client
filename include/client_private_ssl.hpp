#ifndef CLIENT_PRIVATE_SSL_HPP
#define CLIENT_PRIVATE_SSL_HPP

#include <boost/asio.hpp>
#include <boost/asio/ssl/context.hpp>
#include <boost/asio/ssl/stream.hpp>
#include <memory>
#include "client_private.hpp"
#if BOOST_OS_WINDOWS
#include <wincrypt.h>
#endif

namespace simple_http {

boost::asio::ssl::context ctx{boost::asio::ssl::context::sslv23_client};

#if BOOST_OS_WINDOWS
void add_windows_root_certs(boost::asio::ssl::context &ctx) {
  HCERTSTORE hStore = CertOpenSystemStoreA(0, "ROOT");
  if (hStore == NULL) {
    return;
  }

  X509_STORE *store = X509_STORE_new();
  PCCERT_CONTEXT pContext = NULL;
  while ((pContext = CertEnumCertificatesInStore(hStore, pContext)) != NULL) {
    X509 *x509 =
        d2i_X509(NULL, (const unsigned char **)&pContext->pbCertEncoded, pContext->cbCertEncoded);
    if (x509 != NULL) {
      X509_STORE_add_cert(store, x509);
      X509_free(x509);
    }
  }

  CertFreeCertificateContext(pContext);
  CertCloseStore(hStore, 0);

  SSL_CTX_set_cert_store(ctx.native_handle(), store);
}
#endif

void prepareSslContext() {
  static bool prepared = false;
  if (prepared) {
    return;
  }
  namespace ssl = boost::asio::ssl;
  ctx.set_options(ssl::context::default_workarounds | ssl::context::no_sslv2
                  | ssl::context::no_sslv3 | ssl::context::tlsv12_client);
  // The below code validates root certificates based on the OS standard methods.
#if BOOST_OS_WINDOWS
  add_windows_root_certs(ctx);
#else
  ctx.set_default_verify_paths();
#endif
  prepared = true;
}

template <typename RequestBody, typename ResponseBody> class basic_client;

template <typename RequestBody, typename ResponseBody>
class client_private_ssl
    : public client_private<RequestBody, ResponseBody>,
      public std::enable_shared_from_this<client_private_ssl<RequestBody, ResponseBody>> {
public:
  explicit client_private_ssl(boost::asio::io_context &io,
                              std::shared_ptr<basic_client<RequestBody, ResponseBody>> cl)
      : client_private<RequestBody, ResponseBody>{io, cl}, m_stream{io, ctx} {
    prepareSslContext();
  }

private:
  boost::asio::ssl::stream<boost::asio::ip::tcp::socket> m_stream;

  inline std::shared_ptr<client_private<RequestBody, ResponseBody>> shared_from_this() {
    return std::enable_shared_from_this<client_private_ssl<RequestBody,
                                                           ResponseBody>>::shared_from_this();
  }

  inline std::shared_ptr<client_private_ssl<RequestBody, ResponseBody>> self() {
    return std::enable_shared_from_this<client_private_ssl<RequestBody,
                                                           ResponseBody>>::shared_from_this();
  }

  void connectSocket() {
    this->resetTimeout(Connect);
    boost::asio::async_connect(
        m_stream.next_layer(), this->m_resolveResults.begin(), this->m_resolveResults.end(),
        std::bind(&client_private_ssl<RequestBody, ResponseBody>::onSslConnect, self(),
                  std::placeholders::_1));
  }

  void onSslConnect(boost::system::error_code ec) {
    if (ec) {
      fail(ConnectionError, "Error connecting: " + ec.message());
      return;
    }
    // Perform the SSL handshake
    m_stream.async_handshake(boost::asio::ssl::stream_base::client,
                             std::bind(&client_private_ssl<RequestBody, ResponseBody>::onHandshake,
                                       self(), std::placeholders::_1));
  }

  void onHandshake(boost::system::error_code ec) {
    if (ec) {
      fail(HandshakeError, "Error during handshake: " + ec.message());
      return;
    }
    sendRequest();
  }

  void sendRequest() {
    this->resetTimeout(RequestSend);
    this->clearResponse();
    // Send the HTTP request to the remote host
    boost::beast::http::async_write(
        m_stream, this->m_request, std::bind(&client_private_ssl<RequestBody, ResponseBody>::onWrite,
                                       self(), std::placeholders::_1, std::placeholders::_2));
  }

  void initiateReadHeader() {
    this->resetTimeout(Header);
    // Receive the HTTP response
    boost::beast::http::async_read_header(
        m_stream, this->m_buffer, *(this->m_responseParser),
        std::bind(&client_private_ssl<RequestBody, ResponseBody>::onReadHeader, self(),
                  std::placeholders::_1, std::placeholders::_2));
  }

  void initiateRead() {
    this->resetTimeout(Contents);
    // Receive the HTTP response
    boost::beast::http::async_read(m_stream, this->m_buffer, *(this->m_responseParser),
                                   std::bind(&client_private_ssl<RequestBody, ResponseBody>::onRead,
                                             self(), std::placeholders::_1, std::placeholders::_2));
  }
  void closeSocket(boost::system::error_code &ec) {
    // Gracefully close the stream
    m_stream.async_shutdown(
        std::bind(&client_private_ssl::onShutdown, self(), std::placeholders::_1));
  }
  void onShutdown(boost::system::error_code ec) {
    if (ec && ec != boost::asio::error::eof) {
      c->failure(Unknown, "Unexpected error on shutdown: " + ec.message());
    }
    // If we get here then the connection is closed gracefully
  }
};
}

#endif // CLIENT_PRIVATE_SSL_HPP
