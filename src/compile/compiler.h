//
// Created by PC on 2026/6/15.
//

#ifndef RTFS2D_COMPILER_H
#define RTFS2D_COMPILER_H
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace rtfs2d {

class AbstractInput;

class Token {
public:
    enum MatchResult {
        Accept,
        Ignore,
        Error
    };
    virtual int Type() = 0;
    virtual MatchResult OnMatched(const std::string& lexeme, AbstractInput& input) = 0;
    virtual ~Token() = default;
    template<class T> T& Cast() {
        return static_cast<T&>(*this);
    }
};

class TokenFileEnd final : public Token{
public:
    TokenFileEnd() = default;
    int Type() override;
    MatchResult OnMatched(const std::string& lexeme, AbstractInput& input) override;
};

class TokenSingle : public Token{
protected:
    char ch{};
public:
    int Type() override;
    MatchResult OnMatched(const std::string& lexeme, AbstractInput& input) override;
};

class Symbol{
public:
    bool is_terminal_;
    int id_;
    Symbol(bool is_terminal, int id): is_terminal_(is_terminal), id_(id) {}
};

class Production final {
public:
    int id_;
    int body_len_;
    std::shared_ptr<Symbol> head_;
    std::vector<std::shared_ptr<Symbol>> body_;

    Production(int id, std::shared_ptr<Symbol> head, int body_len,
        std::vector<std::shared_ptr<Symbol>> body)
        : id_(id), body_len_(body_len),head_(std::move(head)), body_(std::move(body)) {}
    ~Production() = default;
};

class Property {
public:
    virtual void OnReduced(
        const std::unique_ptr<Production>& p,
        const std::vector<std::unique_ptr<Property>>& properties) = 0;
    virtual ~Property() = default;

    template<typename T>
    std::unique_ptr<T> CastProp(std::unique_ptr<Property> prop) {
        auto* ptr = prop.get();
        prop.release();
        return std::unique_ptr<T>(ptr);
    }
};

class PropertyTerminal final : public Property {
public:
    explicit PropertyTerminal(std::unique_ptr<Token> token): token_(std::move(token)){}
    ~PropertyTerminal() override = default;
    void OnReduced(const std::unique_ptr<Production>& p,
        const std::vector<std::unique_ptr<Property>>& properties) override {};
    Token& token() const { return *token_; };
private:
    std::unique_ptr<Token> token_;
};

class Lexer {
public:
    virtual std::unique_ptr<Token> NextToken(AbstractInput& input) noexcept;
    virtual ~Lexer() = default;
};

class DFALexer : public Lexer {
public:
    typedef std::function<std::unique_ptr<Token>()> TokenSupplier;
    DFALexer(int states_count, int start_state);
    std::unique_ptr<Token> NextToken(AbstractInput& input) noexcept override;
    ~DFALexer() override = default;
protected:
    int states_count_;
    int start_state_;
    std::vector<bool> accepted_;
    std::vector<std::vector<int>> goto_;
    std::vector<TokenSupplier> token_suppliers_;
};


class LALRParser {
public:
    typedef std::function<std::unique_ptr<Property>()> PropertySupplier;
    LALRParser(int remap, int non_terminal, int terminal, int states);
    void Run(Lexer& lexer, AbstractInput& input);
    virtual ~LALRParser();
protected:
    virtual void OnFinished() {};
    virtual void OnFailed(const std::unique_ptr<Token>& at);
    virtual void OnReduced() {};
    virtual void OnShifted() {};

    int states_count_;
    std::vector<std::vector<int>> actions_;
    std::vector<std::vector<int>> goto_;
    std::vector<int> terminal_remap_;
    std::vector<std::unique_ptr<Production>> productions_;
    std::vector<PropertySupplier> suppliers_;
    std::vector<std::shared_ptr<Symbol>> symbols_;
    AbstractInput* input_;
};

}


#endif //RTFS2D_COMPILER_H
