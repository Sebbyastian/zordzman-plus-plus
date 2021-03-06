#pragma once

#include <functional>
#include <map>
#include <queue>
#include <string>
#include <tuple>
#include <vector>

#include <sys/socket.h>
#include <sys/types.h>

#include "common/extlib/json11/json11.hpp"

/// Networking utilities common to both the server and client
namespace net {

typedef std::string MessageType;
typedef json11::Json MessageEntity;
typedef int Socket;

/// Handle sending and receiving JSON-encoded messages over a socket.
///
/// This class operators on a TCP socket to communicate whitespace-separated,
/// JSON-encoded messages. Each 'message' is a JSON object with two fields.
/// The `type` field is a string that identifies the type of the message and
/// is used to map messages to callbacks.
///
/// Secondly, the `entity` field can be any JSON type. A string, a number or
/// even a complex data structure comprised of multiple nested objects and
/// arrays. Anything is valid in this field. However the structure of the
/// `entity` field is implied by the value of the `type` field. As such, all
/// messages of a given type should conform to a defined structure.
///
/// Note that this class provides no mechanism for validating message entities
/// so message handlers must implement that themselves.
///
/// @code{.json}
/// {"type": "example", "entity": ...}
/// @endcode
///
/// As mentioned above, this class introduces a concept of callbacks/handlers.
/// These are void-returning callable objects that are called in response to
/// messages of certain types being received. Handlers must be registered on
/// `MessageProcessor` instances via `MessageProcessor::addHandler`.
///
/// This class is generic and can deal with handlers with any signature by
/// specifying the handler signature in the type declaration. For example:
///
/// @code
/// net::MessageProcessor<int, std::string>
/// @endcode
///
/// Handlers for the above message processor will accept an `int` and
/// `std::string` argument. However, as well as this, there are two implicit
/// leading arguments. The first is a pointer to the `MessageProcessor` that
/// the handler is registered on and the second is the message entity field
/// as a `json11::Json` (for which the `MessageEntity` typedef is provided
/// for convenience). Therefore, a handler for the above message processor
/// would look like:
///
/// @code
/// typedef net::MessageProcessor<int, std::String> Processor;
///
/// void handler(Processor *processor,
///              net::MessageEntity entity, int first, std::string second) {
///       processor->send("echo", entity);
///  }
///
/// Processor processor(...);
/// processor.addHandler("example", handler);
/// processor.dispatch(5, "foo");
/// @endcode
///
/// The handlers can use the processor pointer in order to `send` responses.
///
/// There is a slight variation on handlers which are referred to as '*muted
/// handlers*'. Muted handlers are the same as regular handlers except they
/// don't accept the initial pointer to the processor instance, therefore they
/// are unable to `send` responses. Continuing from the above example:
///
/// @code
/// void muted_handler(net::MessageEntity entity,
///                    int first, std::string second) {
///     // Mmph mphna mprh.
/// }
///
/// processor.addHandler("example", muted_handler);
/// processor.dispatch(5, "foo");
/// @endcode
template <class ... Args> class MessageProcessor {

using Handler = std::function<void(
    MessageProcessor<Args ...> *,
    MessageEntity,
    Args ...
)>;

using MutedHandler = std::function<void(
    MessageEntity,
    Args ...
)>;

public:
    /// @param socket A connected socket descriptor
    MessageProcessor(Socket socket) {
        m_socket = socket;
        m_buffer.reserve(8192);
    }

    /// Register a callback for a given message type
    ///
    /// The callback -- or rather, the *handler* -- should accept two implicit
    /// initial arguments. The first is a pointer to the calling message
    /// proccesor instance, and the second is the MessageEntity for the message
    /// the handler is called for.
    ///
    /// When a message is received that has a type that matches the one the
    /// handler is registered against, then the handler is called being passed
    /// the message's entity as the second parameter.
    ///
    /// Multiple handlers can be registered for a single type. Each handler is
    /// called once for each message received.
    void addHandler(MessageType type, Handler handler) {
        m_handlers[type].push_back(handler);
    }

    /// Register a muted callback for a given message type
    ///
    /// Muted handlers are the same as other handlers except they don't
    /// accept a pointer to the message processor as the first argument.
    /// Because of this they are unable to send messages back, effectively
    /// making them read-only or 'muted'.
    ///
    /// This mostly exists to save keystrokes.
    void addHandler(MessageType type, MutedHandler handler) {
        addHandler(type, [handler](MessageProcessor<Args ...> *processor,
                MessageEntity entity, Args ... args){
            return handler(entity, args ...);
        });
    }

    /// Call all handlers for recieved messages
    ///
    /// This will call all the handlers for each message that has been received
    /// by calls to `process`. The given `args` are passed through to the
    /// handler calls.
    void dispatch(Args ... args) {
        while (!m_ingress.empty()) {
            for (auto &handler : m_handlers[std::get<0>(m_ingress.front())]) {
                handler(this, std::get<1>(m_ingress.front()), args ...);
            }
            m_ingress.pop();
        }
    }

    /// Receive and parse messages
    ///
    /// This will attempt to receive JSON-endoded messages from the associated
    /// socket. Note that this method doesn't call the message handlers
    /// immediately. Instead they are enqueued for deferred dispatching via
    /// `dispatch`.
    ///
    /// The order the messages are recevied is the same order they'll be
    /// dispatched.
    void proccess() {
        // TODO: Propagation of errors
        auto free_buffer = m_buffer.capacity() - m_buffer.size();
        if (free_buffer == 0) {
            // What do?
            return;
        }
        ssize_t data_or_error = recv(m_socket,
             m_buffer().data() + m_buffer.size(), free_buffer);
        if (data_or_error == 0) {
            return;
        } else if (data_or_error == -1) {
            // Error, need to check errno, may be EAGAIN/EWOULDBLOCK
            return;
        }
        parseBuffer();
    }

    /// Enqueue a message to be sent
    ///
    /// The message will be encoded as a JSON object with two fields: the
    /// `type` and `entity` which will be set to given corresponding
    /// parameters.
    ///
    /// Note that this doesn't necessarily result in the message being sent
    /// immediately. Rather a buffer of pending messages is maintained which
    /// can be sent by a call to `flushSendQueue`.
    ///
    /// The order in which messages are enqueued is guarateed to be the order
    /// they arrive at host they're sent to.
    void send(MessageType type, MessageEntity entity) {
        m_egress.emplace(type, entity);
    }

    /// Encode and send all enqueued messages
    ///
    /// Each JSON message that has been enqueued by send() is encoded into JSON
    /// and is sent over the associated socket with a null terminator.
    ///
    /// This consumes the send queue entirely.
    void flushSendQueue() {
        while (!m_egress.empty()) {
            json11::Json message = json11::Json::object{
                { "type", std::get<0>(m_egress.front()) },
                { "entity", std::get<1>(m_egress.front()) },
            };
            m_egress.pop();
            std::string encoded_message = message.dump() + " ";
            int sent = 0;
            while (sent < encoded_message.size()) {
                ssize_t data_or_error = ::send(m_socket,
                                             encoded_message.data() + sent,
                                             encoded_message.size() - sent, 0);
                if (data_or_error == -1) {
                    // TODO: Handle/propagate error
                } else {
                    sent = sent + data_or_error;
                }
            }
        }
    }

private:
    Socket m_socket;
    std::string m_buffer;
    std::map<MessageType, std::vector<Handler>> m_handlers;
    std::queue<std::tuple<MessageType, MessageEntity>> m_ingress;
    std::queue<std::tuple<MessageType, MessageEntity>> m_egress;

    /// Attempt to parse all JSON-encoded messages from the buffer
    ///
    /// This parses all whitespace-delimited JSON objects from the buffer and
    /// calls and adds them to the m_ingress message queue to be dispatched
    /// later.
    ///
    /// Each JSON message should be an object at the top level with a string
    /// `type` field. There should also be a `entity` field which can be of any
    /// type. It is this entity field object that's passed to the message
    /// handler so it's up to the handler to determine validity.
    ///
    /// If a JSON message is not an object, missing the 'type' field or the
    /// type field is the wrong type then the message is ignored. The buffer
    /// will still be consumed as if it were a valid message.
    ///
    /// If the buffer contains incomplete or malformed JSON then no messages
    /// are processed. No messages are added to `m_ingress`. The buffer is not
    /// consumed.
    void parseBuffer() {
        if (m_buffer.empty()) {
            return;
        }
        std::string json_error;
        std::vector<json11::Json> messages =
            json11::Json::parse_multi(m_buffer, json_error);
        // Parsing will fail if the buffer contains a partial message, so the
        // JSON may be well formed but incomplete. This is not ideal. Ideally
        // we should be able to read all the complete messages and only leave
        // the incomplete one in the buffer. This may be an argument in favour
        // of not using `parse_multi`.
        if (json_error.size()) {
            // TODO: Log JSON decode error?
        } else {
            m_buffer.clear();
            for (auto &message : messages) {
                json11::Json type = message["type"];
                // If the 'type' field doesn't exist then is_string()
                // is falsey
                if (type.is_string()) {
                    m_ingress.emplace(type.string_value(), message["entity"]);
                }
            }
        }
    }
};

}  // namesapce net
