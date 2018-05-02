#pragma once

#include <string>

class UserButton {

public:
  std::wstring label_;
  std::wstring language_tag_;
  std::wstring image_mso_;
  std::wstring tip_;
  uint32_t id_;

public:
  UserButton(
    const std::wstring &label = L"",
    const std::wstring &language_tag = L"",
    const std::wstring &image_mso = L"",
    const std::wstring &tip = L"",
    const int id = 0) {

    label_ = label;
    language_tag_ = language_tag;
    image_mso_ = image_mso;
    tip_ = tip;
    id_ = id;

  }

  UserButton(const UserButton &rhs)
    : label_(rhs.label_)
    , language_tag_(rhs.language_tag_)
    , image_mso_(rhs.image_mso_)
    , tip_(rhs.tip_)
    , id_(rhs.id_)
  {}

};
