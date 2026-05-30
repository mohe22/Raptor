
#include <array>
#include <cstring>
#include <print>
#include <sodium.h>
#include <sodium/crypto_kx.h>
#include <sodium/version.h>
#include <vector>


using uchar = unsigned char;

struct Client{
    uchar pk[crypto_kx_PUBLICKEYBYTES]; // public key
    uchar sk[crypto_kx_SECRETKEYBYTES]; // private key
    // to decrypt incmong
    uchar rx[crypto_kx_SESSIONKEYBYTES]; // receive session key

    // to encrypt outgoing
    uchar tx[crypto_kx_SESSIONKEYBYTES]; // transmit session key

    Client(){
        // generate key pair (public key, private key)
        crypto_kx_keypair(pk, sk);
    }
    bool deriveSessionKeys(const uchar* server_pk){
        // generate session key
        if (crypto_kx_client_session_keys(rx, tx, pk, sk, server_pk) != 0) {
            std::println("Client: key exchange failed!");
            return false;
        }
        return true;
    }

    std::vector<uchar> encrypt(const std::string&msg) const {
        // 1. generate nonce 24 bytes;
        uchar nonce[crypto_secretbox_NONCEBYTES];
        randombytes_buf(nonce, sizeof(nonce));
        // 2. allocate space for encrypted data
        std::vector<uchar> encrypted(
            crypto_secretbox_NONCEBYTES +       // 24 bytes — nonce
            crypto_secretbox_MACBYTES +   // 16 bytes — authentication tag
            msg.size()                    // N  bytes — actual ciphertext
        );
        // [ nonce 24B | mac 16B | ciphertext NB ]
        // ─────────────────────────────────────
        // total = 24 + 16 + msg.size()

        // 3. Copy nonce into buffer

        memcpy(encrypted.data(), nonce, crypto_secretbox_NONCEBYTES);

        // 4. encrypt message
        crypto_secretbox_easy(
            encrypted.data() + crypto_secretbox_NONCEBYTES,
            (const uchar*)msg.data(),
            msg.size(),
            nonce,
            tx
        );
        return encrypted;

    }

    std::string decrypt(const std::vector<uchar>& encrypted) const {

        // 1. extract nonce from front
        //  The nonce is at the very start of the packet (we put it there during encrypt)
        const uchar* nonce = encrypted.data();

        // 2. extract ciphertext (mac + encrypted data) after nonce
        const uchar* ciphertext    = encrypted.data() + crypto_secretbox_NONCEBYTES;
        const size_t ciphertextLen = encrypted.size() - crypto_secretbox_NONCEBYTES;

        // 3. allocate plaintext buffer (ciphertext minus mac)
        std::vector<uchar> msg(ciphertextLen - crypto_secretbox_MACBYTES);

        // 4. verify mac + decrypt
        if (crypto_secretbox_open_easy(msg.data(), ciphertext, ciphertextLen, nonce, rx) != 0) {
            std::println("decryption failed / message tampered!");
            return "";
        }

        return std::string(msg.begin(), msg.end());
    }
};

struct Server {
    unsigned char pk[crypto_kx_PUBLICKEYBYTES];
    unsigned char sk[crypto_kx_SECRETKEYBYTES];
    unsigned char rx[crypto_kx_SESSIONKEYBYTES]; // receive: A → B
    unsigned char tx[crypto_kx_SESSIONKEYBYTES]; // send:    B → A

    Server() { crypto_kx_keypair(pk, sk); }

    void deriveSessionKeys(const unsigned char* client_pk) {
        if (crypto_kx_server_session_keys(rx, tx, pk, sk, client_pk) != 0) {
            std::println("Server: key exchange failed!");
        }
    }

    std::vector<unsigned char> encrypt(const std::string& msg) {
        unsigned char nonce[crypto_secretbox_NONCEBYTES];
        randombytes_buf(nonce, sizeof(nonce));

        std::vector<unsigned char> cipher(
            crypto_secretbox_NONCEBYTES + crypto_secretbox_MACBYTES + msg.size()
        );

        memcpy(cipher.data(), nonce, crypto_secretbox_NONCEBYTES);
        crypto_secretbox_easy(
            cipher.data() + crypto_secretbox_NONCEBYTES,
            (const unsigned char*)msg.data(), msg.size(),
            nonce, tx  // B sends using tx key
        );
        return cipher;
    }

    std::string decrypt(const std::vector<unsigned char>& cipher) {
        const unsigned char* nonce = cipher.data();
        const unsigned char* data  = cipher.data() + crypto_secretbox_NONCEBYTES;
        size_t data_len = cipher.size() - crypto_secretbox_NONCEBYTES;

        std::vector<unsigned char> msg(data_len - crypto_secretbox_MACBYTES);
        if (crypto_secretbox_open_easy(msg.data(), data, data_len, nonce, rx) != 0) {
            std::println("Server: decryption failed!");
            return "";
        }
        return std::string(msg.begin(), msg.end());
    }
};
int main(){
    if (sodium_init() < 0) return 1;

    Client A;
    Server B;

    // Step 1: Exchange public keys (over the network in real life)
    A.deriveSessionKeys(B.pk);  // A uses B's public key
    B.deriveSessionKeys(A.pk);  // B uses A's public key

    // Step 2: A sends encrypted message to B
    auto cipher1 = A.encrypt("Hello B, this is secret!");
    std::println("B received: {}", B.decrypt(cipher1));

    // Step 3: B replies encrypted to A
    auto cipher2 = B.encrypt("Hello A, got your message!");
    std::println("A received: {}", A.decrypt(cipher2));
}
