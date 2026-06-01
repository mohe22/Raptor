#pragma once

#include <cstdint>
#include <array>
#include <cstring>
#include <print>
#include <stdexcept>

namespace Raptor::Common {

    /**
     * @brief Bitmask flags carried in every packet header.
     *
     * Multiple flags can be combined with operator|.
     * uint8_t gives 8 bits — all currently used, add fields to Header
     * before adding more flags here.
     */
    enum class Flags : uint8_t {
        Ack       = 1 << 0,  ///< Receiver acknowledges the packet
        Error     = 1 << 1,  ///< Payload contains an error description
        Binary    = 1 << 2,  ///< Payload is raw binary data
        Text      = 1 << 3,  ///< Payload is UTF-8 text
        KeepAlive = 1 << 4,  ///< Heartbeat — no payload expected
        Success   = 1 << 5,  ///< Operation completed successfully
        LastChunk = 1 << 6,  ///< Final chunk of a file transfer — flush and close
        Metadata  = 1 << 7,  ///< First chunk — payload carries filename/file info


    };

    inline Flags operator|(Flags a, Flags b) noexcept {
        return static_cast<Flags>(static_cast<uint8_t>(a) | static_cast<uint8_t>(b));
    }
    inline Flags operator&(Flags a, Flags b) noexcept {
        return static_cast<Flags>(static_cast<uint8_t>(a) & static_cast<uint8_t>(b));
    }
    inline Flags operator~(Flags a) noexcept {
        return static_cast<Flags>(~static_cast<uint8_t>(a));
    }

    /**
     * @brief Direction of the packet relative to the initiating side.
     */
    enum class PacketDirection : uint8_t {
        Req,   ///< Client → Server request
        Resp,  ///< Server → Client response
    };

    /**
     * @brief Logical purpose of the packet's payload.
     */
    enum class PacketType : uint8_t {
        FileUpload,    ///< Client is sending a file to the server
        FileDownload,  ///< Server is sending a file to the client
        Command,       ///< Payload is a command string / response
        Register,      ///< Payload is a registration request / response
    };

    /// Identifies a logical transfer or command exchange across multiple chunks.
    using PacketId = uint32_t;


    static const char* ToString(PacketType type) noexcept {
        switch (type) {
            case PacketType::FileUpload:   return "FileUpload";
            case PacketType::FileDownload: return "FileDownload";
            case PacketType::Command:      return "Command";
            case PacketType::Register: return "register";
            default:                       return "Unknown";
        }
    }

    static const char* ToString(PacketDirection dir) noexcept {
        switch (dir) {
            case PacketDirection::Req:  return "Req";
            case PacketDirection::Resp: return "Resp";
            default:                    return "Unknown";
        }
    }

    static const char* ToString(Flags flag) noexcept {
        switch (flag) {
            case Flags::Ack:       return "Ack";
            case Flags::Error:     return "Error";
            case Flags::Binary:    return "Binary";
            case Flags::Text:      return "Text";
            case Flags::KeepAlive: return "KeepAlive";
            case Flags::Success:   return "Success";
            case Flags::LastChunk: return "LastChunk";
            case Flags::Metadata:  return "Metadata";
            default:               return "Unknown";
        }
    }


    /**
     * @brief Fixed-size packet header prepended to every payload.
     *
     * Wire layout (22 bytes, little-endian):
     *
     *   Offset  Size  Field
     *   ------  ----  -----
     *        0     4  packetId
     *        4     1  type
     *        5     1  direction
     *        6     1  flags
     *        7     4  payloadSize
     *       11     8  totalSize
     *       19     4  checksum (CRC32 of this chunk's payload)
     *
     * NOTE: all multi-byte fields are written in host byte order.
     * Both ends must be the same endianness (little-endian assumed).
     */
    struct Header {
        PacketId        packetId    = 0;                    ///< Identifies the logical transfer/command
        PacketType      type        = PacketType::Command;  ///< What the payload represents
        PacketDirection direction   = PacketDirection::Req; ///< Who is sending
        Flags           flags       = {};                   ///< Bitmask of Flags (no default None)
        uint64_t        payloadSize = 0;                    ///< Byte count of THIS chunk's body

        static constexpr size_t SerializedSize =
            sizeof(PacketId)  +   // 4  — packetId
            sizeof(uint8_t)   +   // 1  — type
            sizeof(uint8_t)   +   // 1  — direction
            sizeof(uint8_t)   +   // 1  — flags
            sizeof(uint64_t);   // 4  — payloadSize



        void setFlag(Flags f) noexcept { flags = flags | f; }
        void clearFlag(Flags f) noexcept { flags = flags & ~f; }

        /// Returns true if ALL bits in f are set.
        bool hasFlag(Flags f) const noexcept {
            return (flags & f) == f;
        }

        bool isAck() const { return hasFlag(Flags::Ack); }
        bool isError() const { return hasFlag(Flags::Error); }
        bool isBinary() const { return hasFlag(Flags::Binary); }
        bool isText() const { return hasFlag(Flags::Text); }
        bool isKeepAlive() const { return hasFlag(Flags::KeepAlive); }
        bool isSuccess() const { return hasFlag(Flags::Success); }
        bool isLastChunk() const { return hasFlag(Flags::LastChunk); }
        bool isMetadata() const { return hasFlag(Flags::Metadata); }

        /**
         * @brief Serializes the header into a fixed-size byte array.
         * Fields are written in the wire layout order shown above.
         */
        std::array<uint8_t, SerializedSize> serialize() const noexcept {
            std::array<uint8_t, SerializedSize> buf{};
            size_t o = 0;

            auto write = [&]<typename V>(V v) {
                std::memcpy(buf.data() + o, &v, sizeof(V));
                o += sizeof(V);
            };

            write(packetId);
            write(static_cast<uint8_t>(type));
            write(static_cast<uint8_t>(direction));
            write(static_cast<uint8_t>(flags));
            write(payloadSize);

            return buf;
        }

        /**
         * @brief Reconstructs a Header from raw bytes.
         * @throws std::runtime_error if size < SerializedSize.
         */
        static Header deserialize(const uint8_t* data, size_t size) {
            if (size < SerializedSize)
                throw std::runtime_error("Header too small");

            Header h{};
            size_t o = 0;

            auto read = [&]<typename V>(V& v) {
                std::memcpy(&v, data + o, sizeof(V));
                o += sizeof(V);
            };

            read(h.packetId);

            uint8_t type, dir, flags;
            read(type);  h.type      = static_cast<PacketType>(type);
            read(dir);   h.direction = static_cast<PacketDirection>(dir);
            read(flags); h.flags     = static_cast<Flags>(flags);

            read(h.payloadSize);


            return h;
        }


        void print() const {
            std::println("Header {{");
            std::println("  packetId:    {}", packetId);
            std::println("  type:        {}", ToString(type));
            std::println("  direction:   {}", ToString(direction));
            std::print  ("  flags:       ");

            // Print each set flag separated by " | ", or "0x00" if none set
            bool any = false;
            auto printFlag = [&](Flags f) {
                if (hasFlag(f)) {
                    if (any) std::print(" | ");
                    std::print("{}", ToString(f));
                    any = true;
                }
            };

            printFlag(Flags::Ack);
            printFlag(Flags::Error);
            printFlag(Flags::Binary);
            printFlag(Flags::Text);
            printFlag(Flags::KeepAlive);
            printFlag(Flags::Success);
            printFlag(Flags::LastChunk);
            printFlag(Flags::Metadata);

            if (!any) std::print("0x00");
            std::println("");

            std::println("  payloadSize: {}", payloadSize);
            std::println("}}");
        }
    };

} // namespace Raptor::Common
