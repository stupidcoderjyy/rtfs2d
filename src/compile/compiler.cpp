//
// Created by PC on 2026/6/15.
//

#include "compiler.h"
#include "compiler_input.h"
#include "util/abstract_input.h"

namespace rtfs2d {

int TokenFileEnd::Type() {
    return 0;
}

Token::MatchResult TokenFileEnd::OnMatched(const std::string &lexeme, AbstractInput &input) {
    return Accept;
}

int TokenSingle::Type() {
    return ch;
}

Token::MatchResult TokenSingle::OnMatched(const std::string &lexeme, AbstractInput &input) {
    ch = lexeme[0];
    return Accept;
}

std::unique_ptr<Token> Lexer::NextToken(AbstractInput &input) noexcept {
    return {};
}

DFALexer::DFALexer(int states_count, int start_state):
        states_count_(states_count),
        start_state_(start_state),
        accepted_(std::vector(states_count, false)),
        goto_(std::vector(states_count, std::vector(128, 0))),
        token_suppliers_(std::vector<TokenSupplier>(states_count)) {
}

std::unique_ptr<Token> DFALexer::NextToken(AbstractInput &input) noexcept {
    BEGIN:
    input.Skip(' ', '\r', '\n');
    input.Mark();
    if (!input.Available()) {
        return std::make_unique<TokenFileEnd>();
    }
    int state = start_state_;
    int last_accepted = -2;
    int extraLoadedBytes = 0;
    while (input.Available()){
        int b = input.Read();
        if (b < 0) {
            try {
                input.Retract();
                input.ReadUtf();
            } catch (std::runtime_error&) {
                return nullptr; //不接受非UTF字符
            }
            b = 1; //UTF字符视为一个用不到的ASCII控制字符
        }
        state = goto_[state][b];
        if (state == 0) {
            extraLoadedBytes++;
            break;
        }
        if (accepted_[state]) {
            last_accepted = state;
            extraLoadedBytes = 0;
        } else {
            extraLoadedBytes++;
        }
    }
    if (last_accepted < 0 || !token_suppliers_[last_accepted]) {
        input.Approach('\r', ' ', '\t');
        return nullptr;
    }
    input.Retract(extraLoadedBytes);
    input.Mark();
    auto token = token_suppliers_[last_accepted]();
    switch (token->OnMatched(input.Capture(), input)) {
        case Token::Accept:
            return std::move(token);
        case Token::Ignore:
            goto BEGIN;
        default:
            return {};
    }
}

LALRParser::LALRParser(int remap, int non_terminal, int terminal, int states):
        states_count_(states),
        actions_(std::vector(states, std::vector(terminal, 0))),
        goto_(std::vector(states, std::vector(non_terminal, 0))),
        terminal_remap_(std::vector(remap, 0)),
        suppliers_(std::vector<PropertySupplier>(non_terminal)),
        input_() {}

void LALRParser::Run(Lexer &lexer, CompilerInput &input) {
    input_ = &input;
    input.Mark();
    std::vector<int> states;
    std::vector<std::unique_ptr<Property>> properties;
    states.push_back(0);
    auto token = lexer.NextToken(input);
    if (token && token->Type() == 0) {
        return; // EOF
    }
    while (true) {
        int s = states.back();
        int order = token ? actions_[s][terminal_remap_[token->Type()]] : 0;
        int type = order >> 16;
        int target = order & 0xFFFF;
        switch(type) {
            default: {
                input.Recover(false);
                OnFailed(token);
                return;
            }
            case 1: {
                std::vector<std::unique_ptr<Property>> body;
                body.push_back(std::move(properties.back()));
                properties.pop_back();
                auto head = suppliers_[0]();
                try {
                    head->OnReduced(productions_[0], body);
                } catch (std::runtime_error& err) {
                    input.Recover(false);
                    throw input_->ErrorAtMark(err.what());
                }
                OnFinished();
                return;
            }
            case 2: {
                input.Mark();
                states.push_back(target);
                properties.push_back(std::make_unique<PropertyTerminal>(std::move(token)));
                token = lexer.NextToken(input);
                OnShifted();
                break;
            }
            case 3: {
                auto& p = productions_[target];
                std::vector<std::unique_ptr<Property>> body(p->body_len_);
                for (int i = p->body_len_ - 1; i >= 0; i--) {
                    auto& symbol = p->body_[i];
                    if (symbol->id_ < 0) {
                        body[i] = nullptr;
                        continue; //ε
                    }
                    states.pop_back();
                    body[i] = std::move(properties.back());
                    properties.pop_back();
                    if (symbol->is_terminal_) {
                        input.RemoveMark();
                    }
                }
                std::unique_ptr<Property> p_head = suppliers_[p->head_->id_]();
                try {
                    p_head->OnReduced(p, body);
                } catch (std::runtime_error& err) {
                    input.Recover(false);
                    throw input_->ErrorAtMark(err.what());
                }
                properties.push_back(std::move(p_head));
                states.push_back(goto_[states.back()][p->head_->id_]);
                OnReduced();
                break;
            }
        }
    }
}

LALRParser::~LALRParser() = default;

void LALRParser::OnFailed(const std::unique_ptr<Token>& at) {
    throw at ?
          input_->ErrorAtMark("syntax error") :
          input_->ErrorMarkToForward("unknown symbol");
}

}
