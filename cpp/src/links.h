#pragma once
#include <QString>
#include <optional>

// Works out what a clicked link points at.
// Contract: ../shared/spec/fixtures/link_cases.json.
namespace links {

// An absolute non-file URL (https, mailto, …) for the system browser, or
// nullopt if href is a document-relative reference.
std::optional<QString> externalUrl(const QString &href);

// Resolve a relative href against the folder holding docPath. nullopt when
// there's nothing to resolve (empty href, or a bare #fragment). Fragment is
// dropped.
std::optional<QString> documentRelativePath(const QString &href, const QString &docPath);

} // namespace links
