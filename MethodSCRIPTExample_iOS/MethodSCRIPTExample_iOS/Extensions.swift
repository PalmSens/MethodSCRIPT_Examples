//
//  Extensions.swift
//  MethodSCRIPTExample_Swift
//
//  Created by Evert Wiesenekker on 08/12/2019.
//  Copyright © 2019 Evert Wiesenekker. All rights reserved.
//

import Foundation

extension RangeReplaceableCollection where Element: Equatable {
    @discardableResult
    mutating func appendIfNotContains(_ element: Element) -> (appended: Bool, memberAfterAppend: Element) {
        if let index = firstIndex(of: element) {
            return (false, self[index])
        } else {
            append(element)
            return (true, element)
        }
    }
}

extension String {
    var withEscapedNewlines: String {
        self.replacingOccurrences(of: "\n", with: "\\n")
    }
    subscript (bounds: CountableClosedRange<Int>) -> String {
        let start = index(startIndex, offsetBy: bounds.lowerBound)
        let end = index(startIndex, offsetBy: bounds.upperBound)
        return String(self[start...end])
    }

    subscript (i: Int) -> String {
        return self[i ..< i + 1]
    }

    subscript (bounds: CountableRange<Int>) -> String {
        let start = index(startIndex, offsetBy: bounds.lowerBound)
        let end = index(startIndex, offsetBy: bounds.upperBound)
        return String(self[start..<end])
    }
}