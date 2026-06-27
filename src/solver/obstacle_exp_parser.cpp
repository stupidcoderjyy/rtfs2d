//
// Created by PC on 2026/6/16.
//

#include "obstacle_exp_parser.h"

#include <cmath>

#include "compile/compiler_input.h"
#include "obstacle_geometry.h"

namespace rtfs2d {
class CompilerInput;

ObstacleExpLexer::ObstacleExpLexer(): DFALexer(9, 5) {
    goto_[2][95] = 2;
    goto_[4][46] = 6;
    goto_[5][46] = 6;
    goto_[4][70] = 3;
    goto_[4][102] = 3;
    goto_[4][76] = 7;
    goto_[4][108] = 7;
    goto_[5][40] = 1;
    goto_[5][41] = 1;
    goto_[5][44] = 1;
    goto_[8][70] = 3;
    goto_[8][102] = 3;
    for(int i = 48 ; i <= 57 ; i ++) {
        goto_[2][i] = 2;
        goto_[4][i] = 4;
        goto_[5][i] = 4;
        goto_[6][i] = 8;
        goto_[8][i] = 8;
    }
    for(int i = 97 ; i <= 122 ; i ++) {
        goto_[2][i] = 2;
        goto_[5][i] = 2;
    }
    for(int i = 65 ; i <= 90 ; i ++) {
        goto_[2][i] = 2;
        goto_[5][i] = 2;
    }

    accepted_[7] = true;
    accepted_[8] = true;
    for(int i = 1 ; i <= 4 ; i ++) {
        accepted_[i] = true;
    }

    TokenSupplier e2 = [] {return std::make_unique<TokenFloat>();};
    TokenSupplier e3 = [] {return std::make_unique<TokenInt>();};
    token_suppliers_[1] = [] {return std::make_unique<TokenSingle>();};
    token_suppliers_[2] = [] {return std::make_unique<TokenId>(); };
    token_suppliers_[3] = e2;
    token_suppliers_[8] = e2;
    token_suppliers_[4] = e3;
    token_suppliers_[7] = e3;
}

Token::MatchResult TokenInt::OnMatched(const std::string &lexeme, AbstractInput &input) {
    data_ = std::stoi(lexeme);
    return Accept;
}

Token::MatchResult TokenId::OnMatched(const std::string &lexeme, AbstractInput &input) {
    if (lexeme == "C" || lexeme == "c") {
        type_ = kCircle;
    } else if (lexeme == "R" || lexeme == "r") {
        type_ = kRect;
    } else if (lexeme == "P" || lexeme == "p") {
        type_ = kPoint;
    } else {
        return Error;
    }
    return Accept;
}

Token::MatchResult TokenFloat::OnMatched(const std::string &lexeme, AbstractInput &input) {
    if (lexeme.ends_with('f') || lexeme.ends_with('F')) {
        data_ = std::stof(lexeme.substr(0, lexeme.size() - 1));
    } else {
        data_ = std::stof(lexeme);
    }
    return Accept;
}

ObstacleExpParser::ObstacleExpParser(ObstacleGeometry& geometry)
    : LALRParser(131, 5, 7, 13), geometry_(&geometry) {
    InitActions();
    InitGoTo();
    InitGrammar();
    InitOthers();
}

void ObstacleExpParser::OnFailed(const std::unique_ptr<Token> &at) {
    LALRParser::OnFailed(at);
}

constexpr int ACCEPT = 0x10000;
constexpr int SHIFT = 0x20000;
constexpr int REDUCE = 0x30000;

void ObstacleExpParser::InitActions() {
    actions_[0][1] = SHIFT | 2;
    actions_[1][1] = SHIFT | 2;
    actions_[1][0] = ACCEPT;
    actions_[2][2] = SHIFT | 4;
    actions_[3][0] = REDUCE | 1;
    actions_[3][1] = REDUCE | 1;
    actions_[4][5] = SHIFT | 7;
    actions_[4][6] = SHIFT | 8;
    actions_[5][3] = SHIFT | 9;
    actions_[5][4] = SHIFT | 10;
    actions_[10][5] = SHIFT | 7;
    actions_[6][3] = REDUCE | 4;
    actions_[6][4] = REDUCE | 4;
    actions_[10][6] = SHIFT | 8;
    actions_[7][3] = REDUCE | 6;
    actions_[7][4] = REDUCE | 6;
    actions_[8][3] = REDUCE | 7;
    actions_[8][4] = REDUCE | 7;
    actions_[9][0] = REDUCE | 3;
    actions_[9][1] = REDUCE | 3;
    actions_[11][3] = REDUCE | 5;
    actions_[11][4] = REDUCE | 5;
    actions_[12][0] = REDUCE | 2;
    actions_[12][1] = REDUCE | 2;
}

void ObstacleExpParser::InitGoTo() {
    goto_[0][1] = 1;
    goto_[0][2] = 3;
    goto_[1][2] = 12;
    goto_[4][3] = 5;
    goto_[4][4] = 6;
    goto_[10][4] = 11;
}

void ObstacleExpParser::InitOthers() {
    terminal_remap_[128] = 1;
    terminal_remap_[129] = 5;
    terminal_remap_[130] = 6;
    terminal_remap_[40] = 2;
    terminal_remap_[41] = 3;
    terminal_remap_[44] = 4;
    suppliers_[3] = [this]{return std::make_unique<PropertyArgs>(debug_out_);};
    suppliers_[1] = [this]{return std::make_unique<PropertyObs>(debug_out_);};
    suppliers_[2] = [this]{return std::make_unique<PropertyShape>(debug_out_);};
    suppliers_[0] = [this]{return std::make_unique<PropertyRoot>(*geometry_, debug_out_);};
    suppliers_[4] = [this]{return std::make_unique<PropertyArg>(debug_out_);};
}

void ObstacleExpParser::InitGrammar() {
    symbols_.push_back(std::make_unique<Symbol>(false, 3));
    symbols_.push_back(std::make_unique<Symbol>(false, 1));
    symbols_.push_back(std::make_unique<Symbol>(false, 2));
    symbols_.push_back(std::make_unique<Symbol>(false, 0));
    symbols_.push_back(std::make_unique<Symbol>(false, 4));
    symbols_.push_back(std::make_unique<Symbol>(true, 2));
    symbols_.push_back(std::make_unique<Symbol>(true, 3));
    symbols_.push_back(std::make_unique<Symbol>(true, 1));
    symbols_.push_back(std::make_unique<Symbol>(true, 4));
    symbols_.push_back(std::make_unique<Symbol>(true, 6));
    symbols_.push_back(std::make_unique<Symbol>(true, 5));
    productions_.push_back(std::make_unique<Production>(0, symbols_[3].get(), 1, std::vector{symbols_[1].get()})); //root → obs
    productions_.push_back(std::make_unique<Production>(1, symbols_[1].get(), 1, std::vector{symbols_[2].get()})); //obs → shape
    productions_.push_back(std::make_unique<Production>(2, symbols_[1].get(), 2, std::vector{symbols_[1].get(), symbols_[2].get()})); //obs → obs shape
    productions_.push_back(std::make_unique<Production>(3, symbols_[2].get(), 4, std::vector{symbols_[7].get(), symbols_[5].get(), symbols_[0].get(), symbols_[6].get()})); //shape → id ( args )
    productions_.push_back(std::make_unique<Production>(4, symbols_[0].get(), 1, std::vector{symbols_[4].get()})); //args → arg
    productions_.push_back(std::make_unique<Production>(5, symbols_[0].get(), 3, std::vector{symbols_[0].get(), symbols_[8].get(), symbols_[4].get()})); //args → args , arg
    productions_.push_back(std::make_unique<Production>(6, symbols_[4].get(), 1, std::vector{symbols_[10].get()})); //arg → int
    productions_.push_back(std::make_unique<Production>(7, symbols_[4].get(), 1, std::vector{symbols_[9].get()})); //arg → float
}

//args → arg
void PropertyArgs::Reduce0(
        PropertyArg& p0) {
    args_.push_back(p0.value());
}

//args → args , arg
void PropertyArgs::Reduce1(
        PropertyArgs& p0,
        PropertyTerminal& p1,
        PropertyArg& p2) {
    args_ = std::move(p0.args_);
    args_.push_back(p2.value());
}

//obs → shape
void PropertyObs::Reduce0(
        PropertyShape& p0) {
    types_.push_back(p0.function());
    args_.push_back(std::move(p0.args()));
}

//obs → obs shape
void PropertyObs::Reduce1(
        PropertyObs& p0,
        PropertyShape& p1) {
    types_ = std::move(p0.types_);
    args_ = std::move(p0.args_);
    types_.push_back(p1.function());
    args_.push_back(std::move(p1.args()));
}

//shape → id ( args )
void PropertyShape::Reduce0(
        PropertyTerminal& p0,
        PropertyTerminal& p1,
        PropertyArgs& p2,
        PropertyTerminal& p3) {
    function_ = p0.token().Cast<TokenId>().type_;
    args_ = std::move(p2.args());
    debug_out_.push_back("T" + std::to_string(function_));
}

//root → obs
void PropertyRoot::Reduce0(PropertyObs& p0) {
    auto& functions = p0.types();
    auto& args_list = p0.args();
    for (int i = 0; i < functions.size(); ++i) {
        AddPoints(functions[i], args_list[i]);
    }
    geometry_->AddObstacle(points_);
}

void PropertyRoot::AddPoints(FunctionType type, const std::vector<float> &args) {
    if (type == kCircle) {
        if (args.size() == 3) {
            AddCircle(args[0], args[1], args[2], 10);
        } else if (args.size() == 4) {
            AddCircle(args[0], args[1], args[2], args[3]);
        } else {
            throw std::runtime_error("usage: [c|C](x0, y0, r, rad_step[optional])");
        }
    } else if (type == kRect) {
        if (args.size() != 4) {
            throw std::runtime_error("usage: [R|r](x0, y0, width, height)");
        }
        AddRect(args[0], args[1], args[2], args[3]);
    } else if (type == kPoint) {
        if (args.size() != 2) {
            throw std::runtime_error("usage: [P|p](x0, y0)");
        }
        AddPoint(args[0], args[1]);
    }
}

void PropertyRoot::AddCircle(float x0, float y0, float r, float step) {
    step *= M_PI / 180.0f;
    float rad = 0;
    while (rad < 2 * M_PI) {
        float x = x0 + r * std::cos(rad);
        float y = y0 + r * std::sin(rad);
        points_.push_back({x, y});
        rad += step;
    }
}

void PropertyRoot::AddRect(float x, float y, float w, float h) {
    points_.push_back({x, y});
    points_.push_back({x, y + h});
    points_.push_back({x + w, y + h});
    points_.push_back({x + w, y});
}

void PropertyRoot::AddPoint(float x, float y) {
    points_.push_back({x, y});
}

//arg → int
void PropertyArg::Reduce0(PropertyTerminal& p0) {
    int val = p0.token().Cast<TokenInt>().data_;
    debug_out_.push_back("I" + std::to_string(val));
    value_ = static_cast<float>(val);
}

//arg → float
void PropertyArg::Reduce1(PropertyTerminal& p0) {
    float val = p0.token().Cast<TokenFloat>().data_;
    debug_out_.push_back("F" + std::to_string(val));
    value_ = val;
}

void PropertyArgs::OnReduced(const std::unique_ptr<Production> &p,
    const std::vector<std::unique_ptr<Property>> &properties) {
    switch (p->id_) {
        case 4: Reduce0(
                *static_cast<PropertyArg*>(properties[0].get()));
            break;
        case 5: Reduce1(
                *static_cast<PropertyArgs*>(properties[0].get()),
                *static_cast<PropertyTerminal*>(properties[1].get()),
                *static_cast<PropertyArg*>(properties[2].get()));
            break;
    }
}

void PropertyObs::OnReduced(
        const std::unique_ptr<Production> &p,
        const std::vector<std::unique_ptr<Property>> &properties) {
    switch (p->id_) {
        case 1: Reduce0(
                *static_cast<PropertyShape*>(properties[0].get()));
            break;
        case 2: Reduce1(
                *static_cast<PropertyObs*>(properties[0].get()),
                *static_cast<PropertyShape*>(properties[1].get()));
            break;
    }
}

void PropertyShape::OnReduced(
        const std::unique_ptr<Production> &p,
        const std::vector<std::unique_ptr<Property>> &properties) {
    Reduce0(
            *static_cast<PropertyTerminal*>(properties[0].get()),
            *static_cast<PropertyTerminal*>(properties[1].get()),
            *static_cast<PropertyArgs*>(properties[2].get()),
            *static_cast<PropertyTerminal*>(properties[3].get()));
}

void PropertyRoot::OnReduced(
        const std::unique_ptr<Production> &p,
        const std::vector<std::unique_ptr<Property>> &properties) {
    Reduce0(
            *static_cast<PropertyObs*>(properties[0].get()));
}

void PropertyArg::OnReduced(
        const std::unique_ptr<Production> &p,
        const std::vector<std::unique_ptr<Property>> &properties) {
    switch (p->id_) {
        case 6: Reduce0(
                *static_cast<PropertyTerminal*>(properties[0].get()));
            break;
        case 7: Reduce1(
                *static_cast<PropertyTerminal*>(properties[0].get()));
            break;
    }
}

}
