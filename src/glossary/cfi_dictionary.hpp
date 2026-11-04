/*
 * Copyright (C) 2002 - 2021 Devexperts LLC
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// SPDX-License-Identifier: MPL-2.0
// Dictionary adapted from pinned dxfeed-api CFI.java; see spec/glossary/VALUES.md.
constexpr DictionaryValue XATTR_VALUES[] = {
    {'X', "Undefined", "No attribute is defined"},
};
constexpr detail::CfiAttributeData XATTR{"Undefined", "No attribute is defined.", XATTR_VALUES};
constexpr DictionaryValue CATEGORIES_VALUES[] = {
    {'E', "Equities", "Financial instruments representing an ownership interest in an entity or pool of assets."},
    {'D', "Debt Instruments",
     "Financial instruments evidencing moneys owed by the issuer to the holder on terms as specified."},
    {'R', "Entitlements (Rights)",
     "Financial instruments providing the holder the privilege to subscribe to or to receive specific assets on terms "
     "specified."},
    {'S', "Structured Products",
     "Financial products that have pre-defined returns and are linked to one or more underlying price, index or rate "
     "with payment at one or more future dates. Note: added in ISO/DIS 10962."},
    {'T', "Referential Products", "Note: added in ISO/DIS 10962."},
    {'O', "Options",
     "Contracts which grant to the holder either the privilege to purchase or the privilege to sell the assets "
     "specified at a predetermined price or formula at or within a time in the future."},
    {'F', "Futures",
     "Contracts which obligate the buyer to receive and the seller to deliver in the future the assets specified at an "
     "agreed price."},
    {'M', "Others (Miscellaneous)", "Financial instruments which do not meet categories as defined."},
};
constexpr detail::CfiAttributeData CATEGORIES{"Categories", "", CATEGORIES_VALUES};
constexpr DictionaryValue FORM_VALUES[] = {
    {'B', "Bearer", "The owner is not registered in the books of the issuer or of the registrar."},
    {'R', "Registered",
     "Securities are recorded in the name of the owner on the books of the issuer or the issuer's registrar and can "
     "only be transferred to another owner when endorsed by the registered owner."},
    {'N', "Bearer/Registered",
     "Securities issued in both bearer and registered form but with the same identification number."},
    {'Z', "Bearer depository receipt",
     "Receipt - in bearer form - for securities issued in a foreign market to promote trading outside the home country "
     "of the underlying securities."},
    {'A', "Registered depository receipt (e.g. ADR)",
     "Receipt - in registered form ? for securities issued in a foreign market to promote trading outside the home "
     "country of the underlying securities."},
    {'G', "Regulation 144 A",
     "A Securities & Exchange Commission rule modifying a two-year holding period requirement on privately placed "
     "securities to permit qualified institutional buyers to trade these positions among themselves."},
    {'S', "Regulation S",
     "Rights acquired through a contractual agreement between the Investor (Restricted Securities Holder) and the "
     "Company (Issuer). Registration Rights entitle investors to force a company to register the investors' shares of "
     "company stock with the Securities and Exchange Commission (SEC) and state securities commissions. This "
     "registration, in turn, enables the investors to sell their shares to the public."},
    {'H', "Regulation 144 A bearer depositary receipt", ""},
    {'I', "Regulation S bearer depositary receipt", ""},
    {'V', "Regulation 144 A registered depositary receipt", ""},
    {'W', "Regulation S registered depositary receipt", ""},
    {'M', "Others (Miscellaneous)", ""},
};
constexpr detail::CfiAttributeData FORM{"Form", "", FORM_VALUES};
constexpr DictionaryValue EQUITIES_VALUES[] = {
    {'S', "Shares, i.e. common/ordinary",
     "Holders typically being entitled to vote and receive dividends. In the event of liquidation, holders of shares "
     "usually rank behind the entity's creditors and holders of preferred shares."},
    {'P', "Preferred shares",
     "Payment of dividends to holders normally takes preference over the payment of dividends to other classes of "
     "shares. In the event of liquidation, preferred shares normally rank above ordinary shares but behind creditors "
     "of the company."},
    {'R', "Preference shares",
     "Like the preferred shares, preference shares have a prior claim on dividends, and on assets in an event of "
     "corporate liquidation or dissolution. But preferred stock would take precedence over preference stock in respect "
     "of dividends and assets that may be available for distribution."},
    {'C', "Convertible shares",
     "Shares (common/ordinary) that, at the option of the holder, are convertible into other securities, at a "
     "designated rate. The conversion privilege may be perpetual or limited to a specific period."},
    {'F', "Preferred convertible shares",
     "Preferred shares (common/ordinary or preferred) that, at the option of the holder, are convertible into other "
     "securities, usually common shares, at a designated rate. The conversion privilege may be perpetual or limited to "
     "a specified period."},
    {'V', "Preference convertibles shares",
     "Preference shares (common/ordinary or preferred) that, at the option of the holder, are convertible into other "
     "securities, usually common shares, at a designated rate. The conversion privilege may be perpetual or limited to "
     "a specified period."},
    {'U', "Units, i.e. unit trusts/mutual funds/OPCVM/OICVM",
     "Securities representing a portion of assets pooled by investors: run by a management company whose share capital "
     "remains separate from such assets."},
    {'L', "Limited partnership units", "Note: added in ISO/DIS 10962."},
    {'M', "Others (Miscellaneous)", "Equities which do not fit into any of the above Groups."},
};
constexpr detail::CfiAttributeData EQUITIES{
    "Equities", "Financial instruments representing an ownership interest in an entity or pool of assets.",
    EQUITIES_VALUES};
constexpr DictionaryValue EQUITIES_VOTING_VALUES[] = {
    {'V', "Voting", "Each share has one vote."},
    {'N', "Non-voting", "Share has no voting right."},
    {'R', "Restricted voting", "The shareholder may be entitled to less than one vote per share."},
    {'E', "Enhanced voting", "The shareholder is entitled to more than one vote per share."},
};
constexpr detail::CfiAttributeData EQUITIES_VOTING{
    "Voting right", "Indicates the kind of voting power conferred to the shareholder.", EQUITIES_VOTING_VALUES};
constexpr DictionaryValue EQUITIES_TRANSFER_VALUES[] = {
    {'T', "Restrictions", "The ownership or transfer of the security is subject to special conditions."},
    {'U', "Free (Unrestricted)", "The ownership or transfer of the security is not subject to special conditions."},
};
constexpr detail::CfiAttributeData EQUITIES_TRANSFER{"Ownership/transfer restrictions", "", EQUITIES_TRANSFER_VALUES};
constexpr DictionaryValue EQUITIES_PAYMENT_VALUES[] = {
    {'O', "Nil paid", ""},
    {'P', "Partly paid", ""},
    {'F', "Fully paid", ""},
};
constexpr detail::CfiAttributeData EQUITIES_PAYMENT{"Payment status", "", EQUITIES_PAYMENT_VALUES};
constexpr DictionaryValue EQUITIES_REDEMPTION_VALUES[] = {
    {'R', "Redeemable", ""},
    {'E', "Extendible", ""},
    {'T', "Redeemable/extendible", ""},
};
constexpr detail::CfiAttributeData EQUITIES_REDEMPTION{"Redemption", "", EQUITIES_REDEMPTION_VALUES};
constexpr DictionaryValue EQUITIES_INCOME_VALUES[] = {
    {'F', "Fixed Rate Income", "The shareholder periodically receives a stated income."},
    {'C', "Cumulative, Fixed Rate Income",
     "The shareholder periodically receives a stated amount. Dividends not paid in any year accumulate and must be "
     "paid at a later date before dividends can be paid on the common/ordinary shares."},
    {'P', "Participating Income",
     "Preferred shareholders, in addition to receiving their fixed rate of prior dividend, share with the common "
     "shareholders in further dividend distributions and in capital distributions."},
    {'Q', "Cumulative, Participating Income",
     "Shareholders are entitled to dividends in excess of the stipulated preferential rate under specified conditions. "
     "Dividends not paid in any year accumulate and must be paid at a later date before dividends can be paid on the "
     "common/ordinary shares."},
    {'A', "Adjustable Rate Income", "The dividend rate is set periodically, usually based on a certain yield."},
    {'N', "Normal Rate Income",
     "Shareholders are entitled to the same dividends as common/ordinary shareholders but have other privileges, e.g. "
     "as regards distribution of assets upon dissolution."},
};
constexpr detail::CfiAttributeData EQUITIES_INCOME{
    "Income", "Indicates the kind of dividend income the shareholders are entitled to.", EQUITIES_INCOME_VALUES};
constexpr DictionaryValue EQUITIES_UNITS_CLOSED_OPEN_VALUES[] = {
    {'C', "Closed-end",
     "Units are sold on either an organized exchange or in the over-thecounter market and are usually not redeemed."},
    {'O', "Open-end",
     "Funds permanently sell new units to the public and redeem outstanding units on demand, resulting in an increase "
     "or decrease of outstanding capital."},
};
constexpr detail::CfiAttributeData EQUITIES_UNITS_CLOSED_OPEN{
    "Closed/open-end",
    "Indicates whether units are traded or whether funds continually stand ready to sell new units and to redeem the "
    "outstanding units on demand.",
    EQUITIES_UNITS_CLOSED_OPEN_VALUES};
constexpr DictionaryValue EQUITIES_UNITS_DISTRIBUTION_VALUES[] = {
    {'I', "Income funds", "The fund regularly distributes its investment profits."},
    {'G', "Growth funds", "The fund normally reinvests its investment profits."},
    {'M', "Mixed funds", "Investment profits are partly distributed, partly reinvested."},
};
constexpr detail::CfiAttributeData EQUITIES_UNITS_DISTRIBUTION{
    "Distribution policy", "Indicates the fund's normal distribution policy.", EQUITIES_UNITS_DISTRIBUTION_VALUES};
constexpr DictionaryValue EQUITIES_UNITS_ASSETS_VALUES[] = {
    {'R', "Real estate", "Fund invests exclusively in real estate."},
    {'S', "Securities", "Fund invests in securities/financial instruments."},
    {'M', "Mixed-general", "Fund invests in different assets."},
    {'C', "Commodities", "Fund invests exclusively in commodities."},
    {'D', "Derivatives", "Fund invests in derivatives."},
};
constexpr detail::CfiAttributeData EQUITIES_UNITS_ASSETS{
    "Assets", "Indicates the investment policy/objective of the fund as set forth in its prospectus.",
    EQUITIES_UNITS_ASSETS_VALUES};
constexpr DictionaryValue EQUITIES_UNITS_STRATEGY_VALUES[] = {
    {'H', "Hedge fund",
     "Type of investment funds which pursue a total return strategy and usually charge a high performance fee in "
     "addition to annual management charges and initial fees."},
    {'B', "Balanced",
     "Funds that buy a combination of common stock, preferred stock, bonds, and short-term bonds, to provide both "
     "income and capital appreciation while avoiding excessive risk."},
    {'E', "Exchange Traded Funds",
     "Funds that track an index, but can be traded like a stock. ETFs always bundle together the securities that are "
     "in an index."},
    {'S', "Sector fund", "Mutual fund whose investments are in a particular sector of industry or economy."},
    {'N', "Mid-cap fund", "Mutual funds which invest in small / medium sized companies."},
    {'L', "Large-cap fund", "Mutual funds which invest in the largest companies."},
    {'I', "High-yield fund",
     "Mutual funds composed primarily of lower-quality, lower-rated securities which offer higher than average income, "
     "or yield."},
    {'A', "Small-cap fund",
     "Mutual fund that holds the stocks of small capitalized companies as opposed to large 'blue chip' companies."},
    {'M', "Others (Miscellaneous)", ""},
};
constexpr detail::CfiAttributeData EQUITIES_UNITS_STRATEGY{
    "Strategy",
    "Indicates the investment strategy of the fund as set forth in its prospectus. Note: added in ISO/DIS 10962.",
    EQUITIES_UNITS_STRATEGY_VALUES};
constexpr DictionaryValue DEBT_INSTRUMENTS_VALUES[] = {
    {'B', "Bonds",
     "Any interest-bearing or discounted security that normally obliges the issuer to pay the bondholder a contracted "
     "sum of money and to repay the principal amount of the debt."},
    {'C', "Convertible bonds", "A bond that can be converted into other securities."},
    {'W', "Bonds with warrants attached",
     "A bond that is issued together with one or more warrant(s) attached as part of the offer, the warrant(s) "
     "granting the holder the right to purchase a designated security, often the common stock of the issuer of the "
     "debt, at a specified price."},
    {'T', "Medium-term notes",
     "Negotiable debt instruments offered under a program agreement through one or more dealers upon request of the "
     "issuer. The program defines the terms and conditions of the notes."},
    {'Y', "Money market instruments",
     "Financial instruments designated at issuance as such with a short-term life, usually twelve months or less, e.g. "
     "treasury bills, commercial paper."},
    {'A', "Asset-backed securities", "Note: added in ISO/DIS 10962."},
    {'G', "Mortgage-backed securities", "Note: added in ISO/DIS 10962."},
    {'N', "Municipal bonds", "Note: added in ISO/DIS 10962."},
    {'M', "Others (Miscellaneous)", "Debt instruments which do not fit into any of above Groups."},
};
constexpr detail::CfiAttributeData DEBT_INSTRUMENTS{
    "Debt Instruments",
    "Financial instruments evidencing moneys owed by the issuer to the holder on terms as specified.",
    DEBT_INSTRUMENTS_VALUES};
constexpr DictionaryValue DEBT_INTEREST_VALUES[] = {
    {'F', "Fixed rate", "All interest payments are known at issuance and remain constant for the life of the issue."},
    {'Z', "Zero rate/Discounted",
     "No periodical interest payments are made; the interest charge (discount) is the difference between maturity "
     "value and proceeds at time of acquisition."},
    {'V', "Variable",
     "The interest rate is subject to adjustment through the life of the issue; includes graduated, i.e. "
     "step-up/step-down, floating and indexed interest rates."},
};
constexpr detail::CfiAttributeData DEBT_INTEREST{"Type of interest", "", DEBT_INTEREST_VALUES};
constexpr DictionaryValue DEBT_GUARANTEE_VALUES[] = {
    {'T', "Government/Treasury guarantee", "The debt instrument is guaranteed by a federal or state government."},
    {'G', "Guaranteed",
     "The debt instrument is guaranteed by an entity other than the issuer; not a federal or state government."},
    {'S', "Secured",
     "A debt issue against which specific assets are pledged to secure the obligation e.g. mortgage, receivables."},
    {'U', "Unsecured/unguaranteed", "The direct obligations of the issuer rest solely on its general credit."},
};
constexpr detail::CfiAttributeData DEBT_GUARANTEE{
    "Guarantee", "Indicates, in the case of the issuer's insolvency, whether the debt issue is additionally secured.",
    DEBT_GUARANTEE_VALUES};
constexpr DictionaryValue DEBT_REIMBURSEMENT_VALUES[] = {
    {'F', "Fixed maturity", "The principal amount is repaid in full at maturity."},
    {'G', "Fixed maturity with call feature",
     "The issue may be called for redemption prior to the fixed maturity date."},
    {'C', "Fixed maturity with put",
     "The holder may request the reimbursment of his bonds prior to the maturity date."},
    {'D', "Fixed maturity with put and call", ""},
    {'A', "Amortization plan", "Reduction of principal by regular payments."},
    {'B', "Amortization plan with call feature",
     "The redemption of principal may occur as the result of the outstanding portion of the bond being called."},
    {'T', "Amortization plan with put", ""},
    {'L', "Amortization plan with put and call", ""},
    {'P', "Perpetual",
     "The debt instrument has no fixed maturity date and is only due for redemption in the case of the issuer's "
     "liquidation."},
    {'Q', "Perpetual with call feature", "The issue may be called for redemption at some time in the future."},
};
constexpr detail::CfiAttributeData DEBT_REIMBURSEMENT{"Redemption/Reimbursement",
                                                      "Indicates the retirement provisions made for the debt issue.",
                                                      DEBT_REIMBURSEMENT_VALUES};
constexpr DictionaryValue ENTITLEMENTS_VALUES[] = {
    {'A', "Allotment rights",
     "Privileges allotted to existing security holders, entitling them to receive new securities free of charge."},
    {'S', "Subscription rights",
     "Privileges allotted to existing security holders, entitling them to subscribe to new securities at a price "
     "normally lower than the prevailing market price."},
    {'P', "Purchase rights",
     "Anti-takeover device that gives a prospective acquiree?s shareholders the right to buy shares of the firm or "
     "shares of anyone who acquires the firm at a deep discount to their fair market value."},
    {'W', "Warrants",
     "Financial instruments which permit the holder to purchase a specified amount of a financial instrument, "
     "commodity, currency or other during a specified period at a specified price."},
    {'M', "Others (Miscellaneous)", "Entitlements (Rights) which do not fit into any of above Groups."},
};
constexpr detail::CfiAttributeData ENTITLEMENTS{"Entitlements (Rights)",
                                                "Financial instruments providing the holder the privilege to subscribe "
                                                "to or to receive specific assets on terms specified.",
                                                ENTITLEMENTS_VALUES};
constexpr DictionaryValue ENTITLEMENTS_RIGHTS_ASSETS_VALUES[] = {
    {'S', "Ordinary shares", ""},
    {'P', "Preferred shares", ""},
    {'R', "Preference shares", ""},
    {'C', "Convertible shares", ""},
    {'F', "Preferred convertible shares", ""},
    {'V', "Preference convertible shares", ""},
    {'B', "Bonds", ""},
    {'O', "Others", ""},
};
constexpr detail::CfiAttributeData ENTITLEMENTS_RIGHTS_ASSETS{"Underlying assets", "",
                                                              ENTITLEMENTS_RIGHTS_ASSETS_VALUES};
constexpr DictionaryValue ENTITLEMENTS_WARRANTS_ASSETS_VALUES[] = {
    {'B', "Basket", "The warrant holder is entitled to acquire a package or group of assets."},
    {'S', "Stock-Equities", "The warrant holder is entitled to acquire equity."},
    {'D', "Debt Instruments/Interest Rates", "The warrant holder is entitled to acquire debt instruments."},
    {'T', "Commodities", "The warrant holder is entitled to acquire a specific commodity."},
    {'C', "Currencies",
     "The warrant holder is entitled to acquire a specified amount in a certain currency at a specified exchange "
     "rate."},
    {'I', "Indices",
     "The warrant holder is entitled to acquire a specified amount based on the performance of an index."},
    {'M', "Others (Miscellaneous)", "The warrant holder is entitled to acquire other assets not mentioned above."},
};
constexpr detail::CfiAttributeData ENTITLEMENTS_WARRANTS_ASSETS{
    "Underlying assets", "Indicates the type of underlying assets that the warrant holder is entitled to acquire.",
    ENTITLEMENTS_WARRANTS_ASSETS_VALUES};
constexpr DictionaryValue ENTITLEMENTS_WARRANTS_TYPE_VALUES[] = {
    {'T', "Traditional warrants", "Issued by the issuer of the underlying instrument."},
    {'N', "Naked warrants",
     "Issued by a third party which is not the issuer of the underlying securities to which the warrant refers. The "
     "warrant issuer does not hold as many securities as would be required if all the warrants are exercised."},
    {'C', "Covered warrants",
     "Issued by a third party which is not the issuer of the underlying securities to which the warrant refers. The "
     "warrant issuer holds as many securities as would be required if all the warrants are exercised."},
};
constexpr detail::CfiAttributeData ENTITLEMENTS_WARRANTS_TYPE{
    "Type", "Indicates whether the warrant is issued by the issuer of the underlying instrument or by a third party.",
    ENTITLEMENTS_WARRANTS_TYPE_VALUES};
constexpr DictionaryValue ENTITLEMENTS_WARRANTS_CALL_PUT_VALUES[] = {
    {'C', "Call",
     "As in most cases, the warrant entitles the holder to acquire specific underlying assets during a specified "
     "period at a specified price."},
    {'P', "Put", "The warrant entitles the holder to acquire cash in exchange for specific underlying assets."},
    {'B', "Call and Put", "Warrants with neither call nor put feature, warrants with call and put feature."},
};
constexpr detail::CfiAttributeData ENTITLEMENTS_WARRANTS_CALL_PUT{
    "Call/Put",
    "Indicates whether the warrant entitles the holder to acquire assets at specified terms or to acquire cash in "
    "exchange for specific underlying assets.",
    ENTITLEMENTS_WARRANTS_CALL_PUT_VALUES};
constexpr DictionaryValue STRUCTURED_PRODUCTS_VALUES[] = {
    {'W', "Structured products with capital protection", "Note: added in ISO/DIS 10962."},
    {'O', "Structured products without capital protection", "Note: added in ISO/DIS 10962."},
};
constexpr detail::CfiAttributeData STRUCTURED_PRODUCTS{
    "Structured Products",
    "Financial products that have pre-defined returns and are linked to one or more underlying price, index or rate "
    "with payment at one or more future dates. Note: added in ISO/DIS 10962.",
    STRUCTURED_PRODUCTS_VALUES};
constexpr DictionaryValue STRUCTURED_ASSETS_VALUES[] = {
    {'B', "Basket", "The option holder is entitled to acquire, respectively to sell a package or group of assets."},
    {'S', "Stock-Equities",
     "The option holder is entitled to acquire, respectively to sell , respectively to sell equity."},
    {'D', "Debt Instruments", "The option holder is entitled to acquire, respectively to sell debt instruments."},
    {'T', "Commodities", "The holder is entitled to acquire, respectively to sell a specific commodity."},
    {'C', "Currencies",
     "The option holder is entitled to acquire, respectively to sell a specified amount in a certain currency at a "
     "specified exchange rate."},
    {'I', "Indices",
     "The option holder is entitled to acquire, respectively to sell a specified amount based on the performance of an "
     "index."},
    {'R', "Ratings", "The holder is entitled to acquire, respectively to sell a specified amount based on ratings."},
    {'N', "Interest rates", "The option's payoff depends on the future level of interest rates."},
    {'M', "Others (Miscellaneous)",
     "The option holder is entitled to acquire, respectively to sell other assets not mentioned above."},
};
constexpr detail::CfiAttributeData STRUCTURED_ASSETS{"Underlying assets",
                                                     "Indicates the type of underlying assets that the holder is "
                                                     "entitled to purchase or sell. Note: added in ISO/DIS 10962.",
                                                     STRUCTURED_ASSETS_VALUES};
constexpr DictionaryValue STRUCTURED_DISTRIBUTION_VALUES[] = {
    {'F', "Fixed interest payments", ""},
    {'D', "Fixed dividend payments", ""},
    {'P', "Fixed premium payments", ""},
    {'I', "Fixed interest and premium payments", ""},
    {'T', "Fixed interest and dividend payments", ""},
    {'V', "Variable interest payments", ""},
    {'E', "Variable dividend payments", ""},
    {'R', "Variable premium payments", ""},
    {'N', "Variable interest and premium payments", ""},
    {'A', "Variable interest and dividend payments", ""},
    {'Y', "No payments", ""},
    {'M', "Others (Miscellaneous)", ""},
};
constexpr detail::CfiAttributeData STRUCTURED_DISTRIBUTION{
    "Distribution", "Indicates the payments the structured product provides for. Note: added in ISO/DIS 10962.",
    STRUCTURED_DISTRIBUTION_VALUES};
constexpr DictionaryValue STRUCTURED_REPAYMENT_VALUES[] = {
    {'F', "Fixed cash repayment", ""},        {'V', "Variable cash repayment", ""},    {'S', "Repayment in stock", ""},
    {'C', "Repayment in stock and cash", ""}, {'T', "Repayment in stock or cash", ""}, {'N', "No repayment", ""},
    {'M', "Others (Miscellaneous)", ""},
};
constexpr detail::CfiAttributeData STRUCTURED_REPAYMENT{
    "Repayment", "Indicates the payments the structured product provides for. Note: added in ISO/DIS 10962.",
    STRUCTURED_REPAYMENT_VALUES};
constexpr DictionaryValue STRUCTURED_STRATEGY_VALUES[] = {
    {'K', "High (unlimited) return potential with sudden death feature for entire product",
     "This product may expire before the indicated expiration date if a predefined knock-out condition (for example a "
     "barrier) has been touched."},
    {'I', "High (unlimited) return potential with sudden lock-in feature for entire product",
     "This product may expire worthless at the indicated expiration date if a predefined knockin condition (for "
     "example a barrier) has not been touched."},
    {'T', "High (unlimited) return potential with static underlying tracking feature",
     "This product has a static component tracking the performance of the underlying instrument. The risk/return "
     "profile has no dynamic features; therefore it mimics the price movements of the underlying instrument."},
    {'R', "High (unlimited) return potential with sudden change feature for part of the product",
     "This product has a static component, tracking the performance of a traditional combination strategy with "
     "unlimited profit/loss profile. However, there is an embedded knock-out or knock-in component, which can lead to "
     "the immediate expiration of parts of the product feature. Therefore, this instrument may possibly change the "
     "strategy features, e.g. a bonus certificate may transform into a tracker certificate, or a twin-win certificate "
     "will transform into a outperformance certificate."},
    {'S', "Limited return potential with static protection feature",
     "This product has a static component with limited risk/return profiles because the embedded capital protection "
     "feature minimizes the loss potential. The profit potential is limited as well because the maximal income is "
     "capped."},
    {'D', "Limited return potential with static high (unlimited) loss potential",
     "This product has a static component with limited return profiles and a theoretically unlimited loss potential "
     "because there is no embedded capital protection feature. The profit potential is limited because the maximal "
     "income is capped."},
    {'B', "Limited return potential with sudden change feature for (unlimited) loss",
     "This product has a static component tracking the performance of a traditionial combination strategy with limited "
     "profit and theoretically unlimited loss potential. However, there is an embedded knock-out or knock-in component "
     "which can lead to the immediate expiration of parts of the product features. Therefore, this instrument may "
     "possibly change the strategy features, e.g. a barrier discount certificate may transform into a discount "
     "certificate, or a barrier reverse convertible may transform into a reverse convertible after the sudden death of "
     "the dynamic portion of the strategy."},
    {'M', "Others (Miscellaneous)", ""},
};
constexpr detail::CfiAttributeData STRUCTURED_STRATEGY{
    "Strategy", "Indicates the strategy used in a structured product. Note: added in ISO/DIS 10962.",
    STRUCTURED_STRATEGY_VALUES};
constexpr DictionaryValue REFERENTIAL_PRODUCTS_VALUES[] = {
    {'C', "Currencies", "Note: added in ISO/DIS 10962."},
    {'T', "Commodities", "Note: added in ISO/DIS 10962."},
    {'R', "Interest rates", "Note: added in ISO/DIS 10962."},
    {'I', "Indices", "Note: added in ISO/DIS 10962."},
    {'S', "Credit Default Swaps", "Note: added in ISO/DIS 10962."},
    {'M', "Others (Miscellaneous)", "Note: added in ISO/DIS 10962."},
};
constexpr detail::CfiAttributeData REFERENTIAL_PRODUCTS{"Referential Products", "Note: added in ISO/DIS 10962.",
                                                        REFERENTIAL_PRODUCTS_VALUES};
constexpr DictionaryValue REFERENTIAL_CURRENCIES_TYPE_VALUES[] = {
    {'N', "National currency", ""},      {'S', "Spot rate", ""}, {'F', "Forward", ""}, {'C', "Coins", ""},
    {'M', "Others (Miscellaneous)", ""},
};
constexpr detail::CfiAttributeData REFERENTIAL_CURRENCIES_TYPE{
    "Type", "Indicates the type or usage of currencies. Note: added in ISO/DIS 10962.",
    REFERENTIAL_CURRENCIES_TYPE_VALUES};
constexpr DictionaryValue REFERENTIAL_COMMODITIES_TYPE_VALUES[] = {
    {'E', "Extraction Resources", "Metals, Precious Metals, Coal, Oil, Gas."},
    {'A', "Agriculture, forestry and fishing", ""},
    {'I', "Industrial Products", "Construction, Manufacturing."},
    {'S', "Services", "Transportation, Communication, Trade."},
};
constexpr detail::CfiAttributeData REFERENTIAL_COMMODITIES_TYPE{
    "Type", "Indicates the type of commodity. Note: added in ISO/DIS 10962.", REFERENTIAL_COMMODITIES_TYPE_VALUES};
constexpr DictionaryValue REFERENTIAL_INTEREST_ANNUITY_VALUES[] = {
    {'F', "Fixed annuity", "Periodical payments are made in a specified amount during a given term."},
    {'V', "Variable annuity",
     "Payments fluctuate in size contingent upon the success of the investment of the principal."},
    {'M', "Others (Miscellaneous)", ""},
};
constexpr detail::CfiAttributeData REFERENTIAL_INTEREST_ANNUITY{
    "Annuity", "Indicates type of interest payments. Note: added in ISO/DIS 10962.",
    REFERENTIAL_INTEREST_ANNUITY_VALUES};
constexpr DictionaryValue REFERENTIAL_INDICES_WEIGHTING_VALUES[] = {
    {'P', "Price weighted", "All index components are weighted by their price."},
    {'W', "Capitalization weighted", "All index components are weighted by their market value."},
    {'C', "Capped capitalization weighted",
     "Index components are weighted by their market value, whereby overweighted index components are underweighted."},
    {'E', "Equal weighted by arithmetic mean",
     "Each index component is taken into account, i.e. the sum of all values is divided by the number of components."},
    {'G', "Equal weighted by geometric mean",
     "Each index component is taken into account, i.e. the nth root of the product of n components."},
};
constexpr detail::CfiAttributeData REFERENTIAL_INDICES_WEIGHTING{
    "Weighting", "Indicates the relative importance of index components. Note: added in ISO/DIS 10962.",
    REFERENTIAL_INDICES_WEIGHTING_VALUES};
constexpr DictionaryValue REFERENTIAL_INDICES_CONSTITUENTS_VALUES[] = {
    {'E', "Equities", ""},
    {'D', "Debt instruments", ""},
    {'T', "Commodities", ""},
    {'M', "Others (Miscellaneous)", ""},
};
constexpr detail::CfiAttributeData REFERENTIAL_INDICES_CONSTITUENTS{
    "Constituents", "Indicates the index components. Note: added in ISO/DIS 10962.",
    REFERENTIAL_INDICES_CONSTITUENTS_VALUES};
constexpr DictionaryValue REFERENTIAL_INDICES_RETURN_VALUES[] = {
    {'P', "Price return", "Not payout adjusted."},
    {'T', "Total return", "Gross dividend, not tax adjusted."},
    {'N', "Net total return", "Net dividend, tax adjusted."},
    {'E', "Excess return", "Total return, adjusted by financing cost or inflation rate."},
    {'B', "Bundled", "Various return calculations integrated in the same instrument."},
};
constexpr detail::CfiAttributeData REFERENTIAL_INDICES_RETURN{
    "Return", "Describes the method applied to the index calculation. Note: added in ISO/DIS 10962.",
    REFERENTIAL_INDICES_RETURN_VALUES};
constexpr DictionaryValue REFERENTIAL_INDICES_HEDGE_VALUES[] = {
    {'C', "Local currency", "Not hedged."},
    {'H', "Hedged", "Currency risk is hedged, but constituent risk is not."},
};
constexpr detail::CfiAttributeData REFERENTIAL_INDICES_HEDGE{
    "Hedge Definition",
    "Specifies whether the index is calculated in local currencies or is currency hedged. Note: added in ISO/DIS "
    "10962.",
    REFERENTIAL_INDICES_HEDGE_VALUES};
constexpr DictionaryValue OPTIONS_VALUES[] = {
    {'C', "Call options",
     "Contracts between a buyer and a seller giving the buyer (holder) the right, but not the obligation, to buy the "
     "assets specified at a fixed price or formula, on or before a specified date. The seller of the call option "
     "assumes the obligation of delivering the assets specified should the buyer exercise his option."},
    {'P', "Put options",
     "Contracts between a buyer and a seller giving the buyer (holder) the right, but not the obligation, to sell the "
     "assets specified at a fixed price or formula, on or before a specified date. The seller of the put option "
     "assumes the obligation of buying the assets specified should the buyer exercise his option."},
    {'M', "Others (Miscellaneous)", "Options which do not fit into any of the above Groups."},
};
constexpr detail::CfiAttributeData OPTIONS{
    "Options",
    "Contracts which grant to the holder either the privilege to purchase or the privilege to sell the assets "
    "specified at a predetermined price or formula at or within a time in the future.",
    OPTIONS_VALUES};
constexpr DictionaryValue OPTIONS_SCHEME_VALUES[] = {
    {'A', "American", "The option can be exercised at any time between its issuance and expiration date."},
    {'E', "European", "The option can be exercised on its expiration date."},
};
constexpr detail::CfiAttributeData OPTIONS_SCHEME{
    "Type of scheme", "Indicates whether an option can be exercised at a specific date or within a defined period.",
    OPTIONS_SCHEME_VALUES};
constexpr DictionaryValue OPTIONS_ASSETS_VALUES[] = {
    {'B', "Basket", "The option gives the right to buy, respectively to sell a package or group of assets."},
    {'S', "Stock-Equities", "The option gives the right to buy, respectively to sell equity."},
    {'D', "Interest rate/notional debt securities",
     "The option gives the right to buy, respectively to sell existing or notional/fictitious debt instruments with a "
     "specific interest rate and maturity."},
    {'T', "Commodities", "The option gives the right to buy, respectively to sell a specific commodity."},
    {'C', "Currencies",
     "The option gives the right to buy, respectively to sell a specified amount in a certain currency at a specified "
     "exchange rate."},
    {'I', "Indices",
     "The option gives the right to buy, respectively to sell a specified amount based on the performance of an "
     "index."},
    {'O', "Options", "The option gives the right to buy, respectively to sell options."},
    {'F', "Futures", "The option gives the right to buy, respectively to sell futures."},
    {'W', "Swaps", "The option gives the right to buy, respectively to sell swaps."},
    {'M', "Others (Miscellaneous)",
     "The option gives the right to buy, respectively to sell other instruments not mentioned above."},
};
constexpr detail::CfiAttributeData OPTIONS_ASSETS{
    "Underlying assets",
    "Indicates the type of underlying assets that the option holder is entitled to buy, respectively to sell.",
    OPTIONS_ASSETS_VALUES};
constexpr DictionaryValue OPTIONS_DELIVERY_VALUES[] = {
    {'P', "Physical", "The underlying instrument must be delivered when the option is exercised."},
    {'C', "Cash", "The settlement of the option is made in cash."},
};
constexpr detail::CfiAttributeData OPTIONS_DELIVERY{"Delivery",
                                                    "Indicates whether the settlement of the option when exercised is "
                                                    "made in cash or whether the underlying instruments are delivered",
                                                    OPTIONS_DELIVERY_VALUES};
constexpr DictionaryValue OPTIONS_STANDARD_VALUES[] = {
    {'S', "Standardized",
     "The underlying instruments, exercise price, expiration date and contract size of the options are standardized. "
     "These options are traded on special option exchanges."},
    {'N', "Non-standardized",
     "The options are custom-made instruments normally sold over the counter. Underlying instruments, strike price, "
     "expiration date and contract size of the options are not standardized."},
};
constexpr detail::CfiAttributeData OPTIONS_STANDARD{
    "Standardized/non-standardized",
    "Indicates whether the terms of options (underlying instruments, strike price, expiration date, contract size) are "
    "standardized or not.",
    OPTIONS_STANDARD_VALUES};
constexpr DictionaryValue FUTURES_VALUES[] = {
    {'F', "Financial Futures", "Futures contracts based on a financial instrument."},
    {'C', "Commodities Futures", "Futures contracts based on bulk goods."},
};
constexpr detail::CfiAttributeData FUTURES{"Futures",
                                           "Contracts which obligate the buyer to receive and the seller to deliver in "
                                           "the future the assets specified at an agreed price.",
                                           FUTURES_VALUES};
constexpr DictionaryValue FUTURES_FINANCIAL_ASSETS_VALUES[] = {
    {'B', "Basket", "The buyer receives, respectively the seller delivers a package or group of assets."},
    {'S', "Stock-Equities", "The buyer receives, respectively the seller delivers equity."},
    {'D', "Interest rate/notional debt securities",
     "The buyer receives, respectively the seller delivers existing or notional debt instruments with a specific "
     "interest rate and maturity."},
    {'C', "Currencies",
     "The buyer receives, respectively the seller delivers a specified amount in a certain currency at a specified "
     "exchange rate."},
    {'I', "Indices",
     "The buyer receives, respectively the seller delivers a specified amount based on the performance of an index."},
    {'O', "Options", "The buyer receives, respectively the seller delivers options."},
    {'F', "Futures", "The buyer receives, respectively the seller delivers futures."},
    {'W', "Swaps", "The buyer receives, respectively the seller delivers swaps."},
    {'M', "Others (Miscellaneous)",
     "The buyer receives, respectively the seller delivers other instruments not mentioned above."},
};
constexpr detail::CfiAttributeData FUTURES_FINANCIAL_ASSETS{
    "Underlying assets",
    "Indicates the type of underlying assets that the futures buyer receives, respectively that the seller delivers.",
    FUTURES_FINANCIAL_ASSETS_VALUES};
constexpr DictionaryValue FUTURES_DELIVERY_VALUES[] = {
    {'P', "Physical", "The underlying instrument must be delivered."},
    {'C', "Cash", "The settlement is made in cash."},
};
constexpr detail::CfiAttributeData FUTURES_DELIVERY{
    "Delivery", "Indicates whether the settlement is made in cash or whether the underlying instruments are delivered.",
    FUTURES_DELIVERY_VALUES};
constexpr DictionaryValue FUTURES_STANDARD_VALUES[] = {
    {'S', "Standardized",
     "The underlying instruments, expiration date and contract size of the futures are standardized. These futures are "
     "traded on special exchanges."},
    {'N', "Non-standardized",
     "Custom-made instruments between two parties. Underlying instruments, expiration date and contract size of the "
     "forwards are not standardized."},
};
constexpr detail::CfiAttributeData FUTURES_STANDARD{
    "Standardized/non-standardized",
    "Indicates whether the terms of the futures (underlying instruments expiration date, contract size) are "
    "standardized or not.",
    FUTURES_STANDARD_VALUES};
constexpr DictionaryValue FUTURES_COMMODITIES_ASSETS_VALUES[] = {
    {'E', "Extraction Resources", "Metals, Precious Metals, Coal, Oil, Gas."},
    {'A', "Agriculture, forestry and fishing", ""},
    {'I', "Industrial Products", "Construction, Manufacturing."},
    {'S', "Services", "Transportation, Communication, Trade."},
};
constexpr detail::CfiAttributeData FUTURES_COMMODITIES_ASSETS{
    "Underlying assets",
    "Indicates the type of underlying assets that the futures buyer receives, respectively that the seller delivers.",
    FUTURES_COMMODITIES_ASSETS_VALUES};
constexpr DictionaryValue OTHERS_VALUES[] = {
    {'C', "Combined Instruments",
     "Financial instruments that are composed of at least two other financial instruments. Note: added in ISO/DIS "
     "10962."},
    {'R', "Referential Instruments",
     "Entities that, in a stricter sense, are not financial instruments but are often made reference to. Note: made "
     "separate category in ISO/DIS 10962."},
    {'M', "Other assets (Miscellaneous)", "Other assets which do not meet groupings as defined."},
};
constexpr detail::CfiAttributeData OTHERS{
    "Others (Miscellaneous)", "Financial instruments which do not meet categories as defined.", OTHERS_VALUES};
constexpr DictionaryValue OTHERS_COMBINED_TYPE_VALUES[] = {
    {'S', "Combination of shares (with different characteristics)", ""},
    {'B', "Combination of bonds (with different characteristics)", ""},
    {'H', "Share(s) and bond(s)", ""},
    {'A', "Share(s) and warrant(s)", ""},
    {'W', "Combination of warrants", ""},
    {'R', "Share(s), bond(s) and warrant(s)", ""},
    {'M', "Other combinations", ""},
};
constexpr detail::CfiAttributeData OTHERS_COMBINED_TYPE{
    "Type of components", "Indicates types of securities combined. Note: added in ISO/DIS 10962.",
    OTHERS_COMBINED_TYPE_VALUES};
constexpr DictionaryValue OTHERS_COMBINED_TRANSFER_VALUES[] = {
    {'T', "Restrictions", "The ownership or transfer of the security is subject to special conditions."},
    {'U', "Free", "Unrestricted; the ownership or transfer of the security is not subject to special conditions."},
};
constexpr detail::CfiAttributeData OTHERS_COMBINED_TRANSFER{
    "Ownership/transfer restrictions", "Note: added in ISO/DIS 10962.", OTHERS_COMBINED_TRANSFER_VALUES};
constexpr DictionaryValue OTHERS_REFERENTIAL_GROUPING_VALUES[] = {
    {'C', "Currencies", ""},
    {'T', "Commodities", ""},
    {'R', "Interest Rates", ""},
    {'I', "Indices", ""},
};
constexpr detail::CfiAttributeData OTHERS_REFERENTIAL_GROUPING{"Further grouping", "",
                                                               OTHERS_REFERENTIAL_GROUPING_VALUES};
constexpr DictionaryValue OTHERS_OTHER_GROUPING_VALUES[] = {
    {'R', "Real Estate Deeds", ""}, {'I', "Insurance Policies", ""},      {'E', "Escrow Receipts", ""},
    {'F', "Forwards", ""},          {'P', "Precious Metal Receipts", ""}, {'M', "Others (Miscellaneous)", ""},
};
constexpr detail::CfiAttributeData OTHERS_OTHER_GROUPING{"Further grouping", "", OTHERS_OTHER_GROUPING_VALUES};

struct GroupEntry {
    char category;
    const detail::CfiAttributeData *attribute;
};

constexpr GroupEntry GROUPS[] = {
    {'E', &EQUITIES},
    {'D', &DEBT_INSTRUMENTS},
    {'R', &ENTITLEMENTS},
    {'S', &STRUCTURED_PRODUCTS},
    {'T', &REFERENTIAL_PRODUCTS},
    {'O', &OPTIONS},
    {'F', &FUTURES},
    {'M', &OTHERS},
};

struct AttributeEntry {
    std::string_view prefix;
    std::array<const detail::CfiAttributeData *, 4> attributes;
};

constexpr AttributeEntry ATTRIBUTES[] = {
    {"ES", {&EQUITIES_VOTING, &EQUITIES_TRANSFER, &EQUITIES_PAYMENT, &FORM}},
    {"EP", {&EQUITIES_VOTING, &EQUITIES_REDEMPTION, &EQUITIES_INCOME, &FORM}},
    {"ER", {&EQUITIES_VOTING, &EQUITIES_REDEMPTION, &EQUITIES_INCOME, &FORM}},
    {"EC", {&EQUITIES_VOTING, &EQUITIES_TRANSFER, &EQUITIES_INCOME, &FORM}},
    {"EF", {&EQUITIES_VOTING, &EQUITIES_REDEMPTION, &EQUITIES_INCOME, &FORM}},
    {"EV", {&EQUITIES_VOTING, &EQUITIES_REDEMPTION, &EQUITIES_INCOME, &FORM}},
    {"EU",
     {&EQUITIES_UNITS_CLOSED_OPEN, &EQUITIES_UNITS_DISTRIBUTION, &EQUITIES_UNITS_ASSETS, &EQUITIES_UNITS_STRATEGY}},
    {"EL", {&EQUITIES_VOTING, &EQUITIES_TRANSFER, &EQUITIES_PAYMENT, &FORM}},
    {"EM", {&XATTR, &XATTR, &XATTR, &FORM}},
    {"DB", {&DEBT_INTEREST, &DEBT_GUARANTEE, &DEBT_REIMBURSEMENT, &FORM}},
    {"DC", {&DEBT_INTEREST, &DEBT_GUARANTEE, &DEBT_REIMBURSEMENT, &FORM}},
    {"DW", {&DEBT_INTEREST, &DEBT_GUARANTEE, &DEBT_REIMBURSEMENT, &FORM}},
    {"DT", {&DEBT_INTEREST, &DEBT_GUARANTEE, &DEBT_REIMBURSEMENT, &FORM}},
    {"DY", {&DEBT_INTEREST, &DEBT_GUARANTEE, &XATTR, &FORM}},
    {"DA", {&DEBT_INTEREST, &DEBT_GUARANTEE, &DEBT_REIMBURSEMENT, &FORM}},
    {"DG", {&DEBT_INTEREST, &DEBT_GUARANTEE, &DEBT_REIMBURSEMENT, &FORM}},
    {"DN", {&DEBT_INTEREST, &DEBT_GUARANTEE, &DEBT_REIMBURSEMENT, &FORM}},
    {"DM", {&DEBT_INTEREST, &DEBT_GUARANTEE, &DEBT_REIMBURSEMENT, &FORM}},
    {"RA", {&XATTR, &XATTR, &XATTR, &FORM}},
    {"RS", {&ENTITLEMENTS_RIGHTS_ASSETS, &XATTR, &XATTR, &FORM}},
    {"RP", {&ENTITLEMENTS_RIGHTS_ASSETS, &XATTR, &XATTR, &FORM}},
    {"RW", {&ENTITLEMENTS_WARRANTS_ASSETS, &ENTITLEMENTS_WARRANTS_TYPE, &ENTITLEMENTS_WARRANTS_CALL_PUT, &FORM}},
    {"RM", {&XATTR, &XATTR, &XATTR, &FORM}},
    {"SW", {&STRUCTURED_ASSETS, &STRUCTURED_DISTRIBUTION, &STRUCTURED_REPAYMENT, &STRUCTURED_STRATEGY}},
    {"SO", {&STRUCTURED_ASSETS, &STRUCTURED_DISTRIBUTION, &STRUCTURED_REPAYMENT, &STRUCTURED_STRATEGY}},
    {"TC", {&REFERENTIAL_CURRENCIES_TYPE, &XATTR, &XATTR, &XATTR}},
    {"TT", {&REFERENTIAL_COMMODITIES_TYPE, &XATTR, &XATTR, &XATTR}},
    {"TR", {&REFERENTIAL_INTEREST_ANNUITY, &XATTR, &XATTR, &XATTR}},
    {"TI",
     {&REFERENTIAL_INDICES_WEIGHTING, &REFERENTIAL_INDICES_CONSTITUENTS, &REFERENTIAL_INDICES_RETURN,
      &REFERENTIAL_INDICES_HEDGE}},
    {"TS", {&XATTR, &XATTR, &XATTR, &XATTR}},
    {"TM", {&XATTR, &XATTR, &XATTR, &XATTR}},
    {"OC", {&OPTIONS_SCHEME, &OPTIONS_ASSETS, &OPTIONS_DELIVERY, &OPTIONS_STANDARD}},
    {"OP", {&OPTIONS_SCHEME, &OPTIONS_ASSETS, &OPTIONS_DELIVERY, &OPTIONS_STANDARD}},
    {"OM", {&XATTR, &XATTR, &XATTR, &XATTR}},
    {"FF", {&FUTURES_FINANCIAL_ASSETS, &FUTURES_DELIVERY, &FUTURES_STANDARD, &XATTR}},
    {"FC", {&FUTURES_COMMODITIES_ASSETS, &FUTURES_DELIVERY, &FUTURES_STANDARD, &XATTR}},
    {"MC", {&OTHERS_COMBINED_TYPE, &OTHERS_COMBINED_TRANSFER, &XATTR, &FORM}},
    {"MR", {&OTHERS_REFERENTIAL_GROUPING, &XATTR, &XATTR, &XATTR}},
    {"MM", {&OTHERS_OTHER_GROUPING, &XATTR, &XATTR, &XATTR}},
};
